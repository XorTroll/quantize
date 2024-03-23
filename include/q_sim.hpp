
#pragma once
#include "base.hpp"
#include "json.hpp"

constexpr size_t CodeStringLength = 10000;
using CodeString = char[CodeStringLength];

constexpr double DefaultHslash = 1.0;
constexpr double DefaultMass = 0.5;
constexpr double DefaultTimeStart = 0.0;
constexpr double DefaultTimeStep = 0.001;
constexpr double DefaultSpaceStart = -1.0;
constexpr double DefaultSpaceEnd = 3.0;
constexpr double DefaultSpaceStep = 0.02;
constexpr double DefaultLeftRegionSeparator = 0.0;
constexpr double DefaultRightRegionSeparator = 0.0;

class QuantumSimulator {
    private:
        double t_0;
        double x_0;
        double x_f;
        double dt;
        double dx;
        double hslash;
        double m;
        double left_region_sep;
        double right_region_sep;
        CodeString psi0_src;
        bool psi0_src_eval;
        bool psi0_src_ok;
        CodeString v_src;
        bool v_src_eval;
        bool v_src_ok;
        long n;
        long cur_ti;
        CVector psi_vec;
        Vector psisq_vec;
        Vector x_vec;
        Vector cur_v_vec;
        std::vector<double> rec_ti;
        std::vector<double> rec_norm;
        std::vector<double> rec_x_est;
        std::vector<double> rec_x2_est;
        std::vector<double> rec_deltax;
        std::vector<double> rec_p_est;
        std::vector<double> rec_p2_est;
        std::vector<double> rec_deltap;
        std::vector<double> rec_deltaprod;
        std::vector<double> rec_energy_est;
        std::vector<double> rec_left_prob;
        std::vector<double> rec_mid_prob;
        std::vector<double> rec_right_prob;

        inline void UpdateSpaceDimensions() {
            this->n = (long)((x_f - x_0) / dx) + 1;
        }

        inline CMatrix CreateEvolutionMatrix() {
            CMatrix q_mat = CMatrix::Zero(this->n, this->n);

            const auto hslash2 = pow(this->hslash, 2);
            const auto dx2 = pow(this->dx, 2);
            
            const auto r = I * ((hslash2 * this->dt) / (4 * dx2 * this->m));

            q_mat(0, 0) = 1.0;
            q_mat(this->n - 1, this->n - 1) = 1.0;

            for(long xi = 1; xi < (this->n - 1); xi++) {
                const auto v_i = this->cur_v_vec(xi) * ((2*m) / hslash2);
                q_mat(xi, xi) = 0.5 * (1.0 + r * (2.0 + dx2 * v_i));
                q_mat(xi, xi - 1) = 0.5 * (-r);
                q_mat(xi, xi + 1) = 0.5 * (-r);
            }

            return q_mat.inverse() - CMatrix::Identity(this->n, this->n);
        }

        inline void CreateXDiscreteVector() {
            this->x_vec = Vector::Zero(this->n);
            for(long xi = 0; xi < this->n; xi++) {
                this->x_vec(xi) = this->DiscreteX(xi);
            }
        }

        bool CreateCurrentVDiscreteVector();

        void UpdateVariableRecords();

    public:
        inline double DiscreteT(const long ti) {
            return this->t_0 + ti * this->dt;
        }

        inline double DiscreteX(const long xi) {
            return this->x_0 + xi * this->dx;
        }

        inline Vector &GetXDiscreteVector() {
            return this->x_vec;
        }

        inline Vector &GetCurrentVDiscreteVector() {
            return this->cur_v_vec;
        }

        inline CVector &GetCurrentPsiDiscreteVector() {
            return this->psi_vec;
        }

        inline Vector &GetCurrentPsiSquareNormDiscreteVector() {
            return this->psisq_vec;
        }

        inline std::vector<double> &GetIterationRecord() {
            return this->rec_ti;
        }

        inline size_t GetRecordSize() {
            return this->rec_ti.size();
        }

        inline std::vector<double> &GetPsiNormRecord() {
            return this->rec_norm;
        }

        inline double GetCurrentPsiNorm() {
            return this->rec_norm.back();
        }

        inline std::vector<double> &GetXEstimateRecord() {
            return this->rec_x_est;
        }

        inline double GetCurrentXEstimateValue() {
            return this->rec_x_est.back();
        }

        inline std::vector<double> &GetXSquaredEstimateRecord() {
            return this->rec_x2_est;
        }
        
        inline double GetCurrentXSquaredEstimateValue() {
            return this->rec_x2_est.back();
        }

        inline std::vector<double> &GetDeltaXRecord() {
            return this->rec_deltax;
        }

        inline double GetCurrentDeltaXValue() {
            return this->rec_deltax.back();
        }

        inline std::vector<double> &GetPEstimateRecord() {
            return this->rec_p_est;
        }

        inline double GetCurrentPEstimateValue() {
            return this->rec_p_est.back();
        }

        inline std::vector<double> &GetPSquaredEstimateRecord() {
            return this->rec_p2_est;
        }
        
        inline double GetCurrentPSquaredEstimateValue() {
            return this->rec_p2_est.back();
        }

        inline std::vector<double> &GetDeltaPRecord() {
            return this->rec_deltap;
        }

        inline double GetCurrentDeltaPValue() {
            return this->rec_deltap.back();
        }

        inline std::vector<double> &GetDeltaProductRecord() {
            return this->rec_deltaprod;
        }

        inline double GetCurrentDeltaProductValue() {
            return this->rec_deltaprod.back();
        }

        inline std::vector<double> &GetEnergyEstimateRecord() {
            return this->rec_energy_est;
        }

        inline double GetCurrentEnergyEstimateValue() {
            return this->rec_energy_est.back();
        }

        inline std::vector<double> &GetLeftRegionProbabilityRecord() {
            return this->rec_left_prob;
        }

        inline double GetCurrentLeftRegionProbability() {
            return this->rec_left_prob.back();
        }

        inline std::vector<double> &GetMiddleRegionProbabilityRecord() {
            return this->rec_mid_prob;
        }

        inline double GetCurrentMiddleRegionProbability() {
            return this->rec_mid_prob.back();
        }

        inline std::vector<double> &GetRightRegionProbabilityRecord() {
            return this->rec_right_prob;
        }

        inline double GetCurrentRightRegionProbability() {
            return this->rec_right_prob.back();
        }

        bool ComputeNextIteration();

        inline void UpdateHslash(const double hslash) {
            this->hslash = hslash;
        }
        inline double GetHslash() {
            return this->hslash;
        }

        inline void UpdateMass(const double m) {
            this->m = m;
        }
        inline double GetMass() {
            return this->m;
        }

        inline void UpdateSpaceStart(const double x_0) {
            this->x_0 = x_0;
            this->UpdateSpaceDimensions();
        }
        inline double GetSpaceStart() {
            return this->x_0;
        }

        inline void UpdateSpaceEnd(const double x_f) {
            this->x_f = x_f;
            this->UpdateSpaceDimensions();
        }
        inline double GetSpaceEnd() {
            return this->x_f;
        }

        inline void UpdateTimeStart(const double t_0) {
            this->t_0 = t_0;
        }
        inline double GetTimeStart() {
            return this->t_0;
        }

        inline void UpdateTimeStep(const double dt) {
            this->dt = dt;
        }
        inline double GetTimeStep() {
            return this->dt;
        }

        inline void UpdateSpaceStep(const double dx) {
            this->dx = dx;
            this->UpdateSpaceDimensions();
        }
        inline double GetSpaceStep() {
            return this->dx;
        }

        inline const char *GetPsi0Source() {
            return this->psi0_src;
        }
        inline void UpdatePsi0Source(const char *src) {
            strcpy(this->psi0_src, src);
            this->psi0_src_eval = false;
            this->psi0_src_ok = false;
        }
        inline bool ComparePsi0Source(const char *src) {
            return strcmp(this->psi0_src, src) == 0;
        }
        inline void NotifyPsi0SourceEvaluated(const bool eval_ok) {
            this->psi0_src_eval = true;
            this->psi0_src_ok = eval_ok;
        }
        inline bool IsPsi0SourceEvaluated() {
            return this->psi0_src_eval;
        }
        inline bool IsPsi0SourceOk() {
            return this->psi0_src_ok;
        }

        inline const char *GetVSource() {
            return this->v_src;
        }
        inline void UpdateVSource(const char *src) {
            strcpy(this->v_src, src);
            this->v_src_eval = false;
            this->v_src_ok = false;
        }
        inline bool CompareVSource(const char *src) {
            return strcmp(this->v_src, src) == 0;
        }
        inline void NotifyVSourceEvaluated(const bool eval_ok) {
            this->v_src_eval = true;
            this->v_src_ok = eval_ok;
        }
        inline bool IsVSourceEvaluated() {
            return this->v_src_eval;
        }
        inline bool IsVSourceOk() {
            return this->v_src_ok;
        }

        inline void UpdateLeftRegionSeparator(const double xl) {
            this->left_region_sep = xl;
        }
        inline double GetLeftRegionSeparator() {
            return this->left_region_sep;
        }

        inline void UpdateRightRegionSeparator(const double xr) {
            this->right_region_sep = xr;
        }
        inline double GetRightRegionSeparator() {
            return this->right_region_sep;
        }

        inline long GetDimensions() {
            return this->n;
        }
        
        inline long GetIteration() {
            return this->cur_ti;
        }

        void Reset();

        void UpdateAll(const double hslash, const double m, const double t_0, const double dt, const double x_0, const double x_f, const double dx);
        bool UpdateFromSettings(const nlohmann::json &settings);
        nlohmann::json GenerateSettings();

        QuantumSimulator(const double hslash, const double m, const double t_0, const double dt, const double x_0, const double x_f, const double dx) {
            this->UpdateAll(hslash, m, t_0, dt, x_0, x_f, dx);
            this->Reset();
        }
};
