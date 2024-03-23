#include "q_sim.hpp"
#include "def_psi0.hpp"
#include "def_v.hpp"

// Due to limitations in C++/JS bindings (in the return types), 3 functions are needed:
// - *_Test to test if the function is properly defined (no exceptions arise and proper return type) for the given position/time
// - *_Real and *_Imaginary to return a complex result in parts (with V the return value is real and this separation is thankfully not needed)

EM_JS(JsResult, sim_Psi0_Test, (const double x), {
    try {
        var psi0_val = psi0(x);
        math.complex(psi0_val);
        return 0;
    }
    catch {
        return 1;
    }
});

EM_JS(double, sim_Psi0_Real, (const double x), {
    return math.complex(psi0(x)).re;
});

EM_JS(double, sim_Psi0_Imaginary, (const double x), {
    return math.complex(psi0(x)).im;
});

EM_JS(JsResult, sim_V_Test, (const double x, const double t), {
    try {
        var v_val = V(x, t);
        if(Number.isFinite(v_val)) {
            return 0;
        }
        else {
            return 2;
        }
    }
    catch {
        return 1;
    }
});

EM_JS(double, sim_V, (const double x, const double t), {
    return V(x, t);
});

namespace {

    bool sim_Psi0_tryGet(const double x, Num &out_psi0) {
        if(JS_RC_SUCCEEDED(sim_Psi0_Test(x))) {
            out_psi0 = Num(sim_Psi0_Real(x), sim_Psi0_Imaginary(x));
            return true;
        }
        else {
            return false;
        }
    }

    bool sim_V_TryGet(const double x, const double t, double &out_v) {
        if(JS_RC_SUCCEEDED(sim_V_Test(x, t))) {
            out_v = sim_V(x, t);
            return true;
        }
        else {
            return false;
        }
    }

}

bool QuantumSimulator::CreateCurrentVDiscreteVector() {
    this->cur_v_vec = Vector::Zero(this->n);

    double cur_v;
    for(long xi = 0; xi < this->n; xi++) {
        if(!sim_V_TryGet(this->DiscreteX(xi), this->DiscreteT(this->cur_ti), cur_v)) {
            this->v_src_ok = false;
            return false;
        }

        this->cur_v_vec(xi) = cur_v;
    }

    return true;
}

void QuantumSimulator::UpdateVariableRecords() {
    this->rec_ti.push_back((double)this->cur_ti);

    // Approximate integrals as finite sums with dx === our discretized space unit (works fine and it's straightforward to implement)
    
    double psi_norm = 0;
    double left_prob = 0;
    double mid_prob = 0;
    double right_prob = 0;
    for(long i = 0; i < this->n; i++) {
        const auto cur_norm_contrib = this->psisq_vec(i) * this->dx;

        psi_norm += cur_norm_contrib;

        if(this->DiscreteX(i) <= this->left_region_sep) {
            left_prob += cur_norm_contrib;
        }
        else if(this->DiscreteX(i) >= this->right_region_sep) {
            right_prob += cur_norm_contrib;
        }
        else {
            mid_prob += cur_norm_contrib;
        }
    }
    left_prob /= psi_norm;
    mid_prob /= psi_norm;
    right_prob /= psi_norm;
    this->rec_norm.push_back(psi_norm);
    this->rec_left_prob.push_back(left_prob);
    this->rec_mid_prob.push_back(mid_prob);
    this->rec_right_prob.push_back(right_prob);
    
    double x_est = 0;
    for(long i = 0; i < this->n; i++) {
        x_est += this->x_vec(i) * this->psisq_vec(i) * this->dx;
    }
    x_est /= psi_norm;
    this->rec_x_est.push_back(x_est);

    double x2_est = 0;
    for(long i = 0; i < this->n; i++) {
        x2_est += pow(this->x_vec(i), 2) * this->psisq_vec(i) * this->dx;
    }
    x2_est /= psi_norm;
    this->rec_x2_est.push_back(x2_est);

    const auto deltax = sqrt(x2_est - pow(x_est, 2));
    this->rec_deltax.push_back(deltax);

    double p_est = 0;
    const CVector p_psi_vec = -I * this->hslash * VectorDerivative(this->psi_vec, this->dx);
    const CVector cj_psi_vec = ConjugatedCVector(this->psi_vec);
    for(long i = 0; i < this->n; i++) {
        // Need to explicitly keep only the real part, even though p is an observable operator thus the result will be real anyway
        p_est += (cj_psi_vec(i) * p_psi_vec(i) * this->dx).real();
    }
    p_est /= psi_norm;
    this->rec_p_est.push_back(p_est);

    double p2_est = 0;
    const CVector p2_psi_vec = - pow(this->hslash, 2) * VectorDDerivative(this->psi_vec, this->dx);
    for(long i = 0; i < this->n; i++) {
        // Same as above
        p2_est += (cj_psi_vec(i) * p2_psi_vec(i) * this->dx).real();
    }
    p2_est /= psi_norm;
    this->rec_p2_est.push_back(p2_est);

    const auto deltap = sqrt(p2_est - pow(p_est, 2));
    this->rec_deltap.push_back(deltap);

    const auto deltaprod = deltax * deltap;
    this->rec_deltaprod.push_back(deltaprod);

    double energy_est = 0;
    for(long i = 0; i < this->n; i++) {
        const Num hm_psi_vec_i = (1.0 / (2.0 * this->m)) * p2_psi_vec(i) + this->cur_v_vec(i) * this->psi_vec(i);
        // Same as above
        energy_est += (cj_psi_vec(i) * hm_psi_vec_i * this->dx).real();
    }
    energy_est /= psi_norm;
    this->rec_energy_est.push_back(energy_est);
}

bool QuantumSimulator::ComputeNextIteration() {
    if(this->cur_ti == 0) {
        this->psi_vec = CVector::Zero(this->n);
        this->CreateXDiscreteVector();

        Num cur_psi0;
        for(long xi = 0; xi < this->n; xi++) {
            if(!sim_Psi0_tryGet(this->DiscreteX(xi), cur_psi0)) {
                this->psi0_src_ok = false;
                return false;
            }
            this->psi_vec(xi) = cur_psi0;
        }

        this->psisq_vec = NormSquaredVector(this->psi_vec);
        if(!this->CreateCurrentVDiscreteVector()) {
            return false;
        }
        this->UpdateVariableRecords();
    }
    else {
        this->psi_vec = this->CreateEvolutionMatrix() * this->psi_vec;

        this->psisq_vec = NormSquaredVector(this->psi_vec);
        if(!this->CreateCurrentVDiscreteVector()) {
            return false;
        }
        this->UpdateVariableRecords();
    }

    this->cur_ti++;
    return true;
}

void QuantumSimulator::Reset() {
    this->cur_ti = 0;
    this->x_vec = {};
    this->cur_v_vec = {};
    this->psi_vec = {};
    this->psisq_vec = {};
    this->rec_ti.clear();
    this->rec_norm.clear();
    this->rec_x_est.clear();
    this->rec_x2_est.clear();
    this->rec_deltax.clear();
    this->rec_p_est.clear();
    this->rec_p2_est.clear();
    this->rec_deltap.clear();
    this->rec_deltaprod.clear();
    this->rec_energy_est.clear();
    this->rec_left_prob.clear();
    this->rec_mid_prob.clear();
    this->rec_right_prob.clear();
    this->psi0_src_eval = false;
    this->psi0_src_ok = false;
    this->v_src_eval = false;
    this->v_src_ok = false;
}

void QuantumSimulator::UpdateAll(const double hslash, const double m, const double t_0, const double dt, const double x_0, const double x_f, const double dx) {
    this->hslash = hslash;
    this->m = m;
    this->t_0 = t_0;
    this->dt = dt;
    this->x_0 = x_0;
    this->x_f = x_f;
    this->dx = dx;
    this->UpdateSpaceDimensions();
    strcpy(this->psi0_src, DefaultPsi0Source);
    this->psi0_src_eval = false;
    this->psi0_src_ok = false;
    strcpy(this->v_src, DefaultVSource);
    this->v_src_eval = false;
    this->v_src_ok = false;
}

bool QuantumSimulator::UpdateFromSettings(const nlohmann::json &settings) {
    #define _GET_ITEM(type, name, def) \
        if(!settings.count(#name)) { \
            return false; \
        } \
        const auto new_##name = settings.value<type>(#name, def);

    _GET_ITEM(double, t_0, DefaultTimeStart);
    _GET_ITEM(double, x_0, DefaultSpaceStart);
    _GET_ITEM(double, x_f, DefaultSpaceEnd);
    _GET_ITEM(double, dt, DefaultTimeStep);
    _GET_ITEM(double, dx, DefaultSpaceStep);
    _GET_ITEM(double, hslash, DefaultHslash);
    _GET_ITEM(double, m, DefaultMass);
    _GET_ITEM(std::string, psi0_src, DefaultPsi0Source);
    _GET_ITEM(std::string, v_src, DefaultVSource);

    this->UpdateAll(new_hslash, new_m, new_t_0, new_dt, new_x_0, new_x_f, new_dx);
    this->UpdatePsi0Source(new_psi0_src.c_str());
    this->UpdateVSource(new_v_src.c_str());
    return true;
}

nlohmann::json QuantumSimulator::GenerateSettings() {
    auto settings = nlohmann::json::object();
    
    #define _SET_ITEM(name) \
        settings[#name] = this->name;

    _SET_ITEM(t_0);
    _SET_ITEM(x_0);
    _SET_ITEM(x_f);
    _SET_ITEM(dt);
    _SET_ITEM(dx);
    _SET_ITEM(hslash);
    _SET_ITEM(m);
    _SET_ITEM(psi0_src);
    _SET_ITEM(v_src);

    return settings;
}
