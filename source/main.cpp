#define GLFW_INCLUDE_ES3
#include <GLES3/gl3.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>

#include "q_sim.hpp"
#include "js_export.hpp"
#include "def_psi0.hpp"
#include "def_v.hpp"

EM_JS(int, GetCanvasWidth, (), {
  return Module.canvas.width;
});

EM_JS(int, GetCanvasHeight, (), {
  return Module.canvas.height;
});

EM_JS(void, ResizeCanvas, (), {
    js_ResizeCanvas();
});

EM_JS(void, OpenUrl, (const char *url), {
    window.open(UTF8ToString(url), "_blank");
});

EM_JS(void, ShowError, (const char *error), {
    // Note: defined in JS since it is also used from there
    js_ShowError(UTF8ToString(error));
});

EM_JS(void, ShowInformation, (const char *info), {
    alert(UTF8ToString(info));
});

EM_JS(int, TryEvaluate, (const char *src), {
    try {
        window.eval(UTF8ToString(src));
        return 0;
    }
    catch(e) {
        return 1;
    }
});

EM_JS(const char*, GetMathJsVersion, (), {
    return stringToNewUTF8(math.version);
});

EM_JS(void, SaveSettingsJson, (const char *settings_json), {
    var pom = document.createElement("a");
    pom.setAttribute("href", "data:text/plain;charset=utf-8," + encodeURIComponent(UTF8ToString(settings_json)));
    pom.setAttribute("download", "quantize_settings.json");

    if(document.createEvent) {
        var event = document.createEvent("MouseEvents");
        event.initEvent("click", true, true);
        pom.dispatchEvent(event);
    }
    else {
        pom.click();
    }
});

EM_JS(void, LoadSimulationSettings, (), {
    var input = document.createElement("input");
    input.type = "file";
    input.id = "file-selector";
    input.accept = ".json";
    input.addEventListener('change', (event) => {
        var file = event.target.files[0];
        
        var reader = new FileReader();
        reader.addEventListener("load", () => {
            var raw_settings = stringToNewUTF8(reader.result);
            Module.ccall("cpp_LoadSettings", null, ["string"], [reader.result]);
        }, false);
        reader.readAsText(file);
    });

    if(document.createEvent) {
        var event = document.createEvent("MouseEvents");
        event.initEvent("click", true, true);
        input.dispatchEvent(event);
    }
    else {
        input.click();
    }
});

namespace {

    // Hard-limit max discretized space dimensions and time iterations, we want to avoid the simulation choking on memory and/or performance as much as possible
    // Note that these are quite arbitrary limits though

    constexpr long MaxSupportedDimensions = 400;
    constexpr long MaxSupportedIterations = 5000;

    constexpr auto SourceGlobalsNoticeText = "NOTE: Simulation variables available: hslash, m, x0, xf, dx, t0, dt";
    constexpr auto SourceFunctionsNoticeText = "NOTE: Special functions available: gauss, delta, hermite (see source demos for usage)";
    constexpr auto SourceLibrariesNoticeText = "NOTE: math.js libraries are used here, check their online docs for more extended usage";
    constexpr auto SourceEvaluationNoticeText = "NOTE: Ψ0 and V sources are globally evaluated (in this order), thus variables defined in Ψ0 source will be overriden by variables in V source with the same name!";

    constexpr auto ClearColor = ImVec4(0.14, 0.14, 0.4, 1.0);
    constexpr auto ErrorColor = ImVec4(0.66, 0.0, 0.0, 1.0);
    constexpr auto NoteColor = ImVec4(0.0, 0.66, 0.5, 1.0);

    QuantumSimulator g_QuantumSimulator(DefaultHslash, DefaultMass, DefaultTimeStart, DefaultTimeStep, DefaultSpaceStart, DefaultSpaceEnd, DefaultSpaceStep);

    bool g_DisplayControlWindow = true;
    bool g_DisplaySourceWindow = true;
    bool g_DisplaySpacePlotWindow = true;
    bool g_DisplaySpaceOpsPlotWindow = false;
    bool g_DisplayMomentumOpsPlotWindow = false;
    bool g_DisplayUncertaintyPlotWindow = false;
    bool g_DisplayEnergyPlotWindow = false;
    bool g_DisplayAboutWindow = false;

    double g_EditHslash = DefaultHslash;
    double g_EditMass = DefaultMass;
    double g_EditTimeStart = DefaultTimeStart;
    double g_EditTimeStep = DefaultTimeStep;
    double g_EditSpaceStart = DefaultSpaceStart;
    double g_EditSpaceEnd = DefaultSpaceEnd;
    double g_EditSpaceStep = DefaultSpaceStep;

    bool g_Running = false;
    bool g_AutoStart = false;
    CodeString g_EditPsi0Source = {};
    CodeString g_EditVSource = {};

    #define _DO_WITH_TEXT_COLOR(color, ...) { \
        ImGui::PushStyleColor(ImGuiCol_Text, color); \
        { __VA_ARGS__ } \
        ImGui::PopStyleColor(); \
    }

    GLFWwindow *g_Window;
    int g_Width;
    int g_Height;

    void EvaluateJsSimulationVariables() {
        #define _EVAL_JS_SIM_VARIABLE(name, val) { \
            const auto js_src = std::string(#name) + " = " + std::to_string(val) + ";"; \
            TryEvaluate(js_src.c_str()); \
        }

        _EVAL_JS_SIM_VARIABLE(hslash, g_QuantumSimulator.GetHslash());
        _EVAL_JS_SIM_VARIABLE(m, g_QuantumSimulator.GetMass());
        _EVAL_JS_SIM_VARIABLE(x0, g_QuantumSimulator.GetSpaceStart());
        _EVAL_JS_SIM_VARIABLE(xf, g_QuantumSimulator.GetSpaceEnd());
        _EVAL_JS_SIM_VARIABLE(dx, g_QuantumSimulator.GetSpaceStep());
        _EVAL_JS_SIM_VARIABLE(t0, g_QuantumSimulator.GetTimeStart());
        _EVAL_JS_SIM_VARIABLE(dt, g_QuantumSimulator.GetTimeStep());
    }

    void ResetSimulation() {
        g_QuantumSimulator.Reset();
        g_Running = g_AutoStart;
    }

    void ResetSimulationToDefault() {
        g_EditHslash = DefaultHslash;
        g_EditMass = DefaultMass;
        g_EditTimeStart = DefaultTimeStart;
        g_EditTimeStep = DefaultTimeStep;
        g_EditSpaceStart = DefaultSpaceStart;
        g_EditSpaceEnd = DefaultSpaceEnd;
        g_EditSpaceStep = DefaultSpaceStep;
        g_QuantumSimulator.UpdateAll(DefaultHslash, DefaultMass, DefaultTimeStart, DefaultTimeStep, DefaultSpaceStart, DefaultSpaceEnd, DefaultSpaceStep);
        strncpy(g_EditPsi0Source, DefaultPsi0Source, __builtin_strlen(DefaultPsi0Source));
        g_EditPsi0Source[sizeof(g_EditPsi0Source) - 1] = '\0';
        strncpy(g_EditVSource, DefaultVSource, __builtin_strlen(DefaultVSource));
        g_EditVSource[sizeof(g_EditVSource) - 1] = '\0';
        ResetSimulation();
    }

    void SaveSimulationSettings() {
        const auto settings = g_QuantumSimulator.GenerateJson();
        const auto settings_json = settings.dump(4);
        SaveSettingsJson(settings_json.c_str());
    }

}

extern "C" EMSCRIPTEN_KEEPALIVE void cpp_LoadSettings(const char *settings_json) {
    try {
        const auto settings = nlohmann::json::parse(settings_json);
        if(g_QuantumSimulator.UpdateFromJson(settings)) {
            g_EditHslash = g_QuantumSimulator.GetHslash();
            g_EditMass = g_QuantumSimulator.GetMass();
            g_EditTimeStart = g_QuantumSimulator.GetTimeStart();
            g_EditTimeStep = g_QuantumSimulator.GetTimeStep();
            g_EditSpaceStart = g_QuantumSimulator.GetSpaceStart();
            g_EditSpaceStep = g_QuantumSimulator.GetSpaceStep();
            g_EditSpaceEnd = g_QuantumSimulator.GetSpaceEnd();
            strcpy(g_EditPsi0Source, g_QuantumSimulator.GetPsi0Source());
            strcpy(g_EditVSource, g_QuantumSimulator.GetVSource());
            ResetSimulation();

            ShowInformation("Successfully loaded settings!");
        }
        else {
            ShowError("Invalid settings JSON!\nSome fields are missing (expected fields: t_0, x_0, x_f, dt, dx, hslash, m, psi0_src, v_src)");
        }
    }
    catch(std::exception &e) {
        std::string error_msg = "Exception while parsing JSON:";
        error_msg += "\n\n";
        error_msg += e.what(); 
        ShowError(error_msg.c_str());
    }
}

extern "C" EMSCRIPTEN_KEEPALIVE double cpp_ApproximateDiracDelta(const double x, const double x0, const double val) {
    // Use simulation's discretized space step value as allowed discrepancy to simulate a Dirac delta
    if(abs(x - x0) <= g_QuantumSimulator.GetSpaceStep()) {
        return val;
    }
    else {
        return 0.0;
    }
}

namespace {

    void OnCanvasDimensionsChanged() {
        glfwSetWindowSize(g_Window, g_Width, g_Height);
        ImGui::SetCurrentContext(ImGui::GetCurrentContext());
    }

    void MainLoop() {
        const auto cur_width = GetCanvasWidth();
        const auto cur_height = GetCanvasHeight();
        if((cur_width != g_Width) || (cur_height != g_Height)) {
            g_Width = cur_width;
            g_Height = cur_height;
            OnCanvasDimensionsChanged();
        }

        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowSize(ImVec2(400, 120), ImGuiCond_Once);
        ImGui::Begin("Main window", nullptr, ImGuiWindowFlags_MenuBar);

        if(ImGui::BeginMenuBar()) {
            if(ImGui::BeginMenu("Simulation")) {
                ImGui::MenuItem("Control", nullptr, &g_DisplayControlWindow);
                if(ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Simulation control and parameters");
                }

                ImGui::MenuItem("Ψ0 and V", nullptr, &g_DisplaySourceWindow);
                if(ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Edit initial state (Ψ0) and potential (V) source code definition");
                }

                ImGui::EndMenu();
            }
            if(ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Tweak various simulation settings");
            }

            if(ImGui::BeginMenu("Plots")) {
                ImGui::MenuItem("Space", nullptr, &g_DisplaySpacePlotWindow);
                if(ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Plot evolution of potential and probability density");
                }

                ImGui::MenuItem("Space operators", nullptr, &g_DisplaySpaceOpsPlotWindow);
                if(ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Plot evolution of position operators");
                }

                ImGui::MenuItem("Momentum operators", nullptr, &g_DisplayMomentumOpsPlotWindow);
                if(ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Plot evolution of momentum operators");
                }

                ImGui::MenuItem("Uncertainty", nullptr, &g_DisplayUncertaintyPlotWindow);
                if(ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Plot uncertainty evolution");
                }

                ImGui::MenuItem("Energy", nullptr, &g_DisplayEnergyPlotWindow);
                if(ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Plot energy evolution");
                }

                ImGui::EndMenu();
            }
            if(ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Space and value/operator evolution plots");
            }

            ImGui::MenuItem("About", nullptr, &g_DisplayAboutWindow);
            if(ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Information about this project");
            }

            ImGui::EndMenuBar();
        }

        ImGui::TextWrapped("Framerate: %.1f FPS", ImGui::GetIO().Framerate);

        if(ImGui::Button("Load")) {
            LoadSimulationSettings();
        }
        
        if(ImGui::Button("Save")) {
            SaveSimulationSettings();
        }

        ImGui::End();

        bool was_reset = false;

        #define _SIM_RESET \
            ResetSimulation(); \
            was_reset = true;

        #define _SIM_RESET_DEFAULT \
            ResetSimulationToDefault(); \
            was_reset = true;

        std::vector<std::string> error_list;

        #define _PUSH_ERROR_FMT(fmt, ...) { \
            char tmp_buf[10000] = {}; \
            snprintf(tmp_buf, sizeof(tmp_buf) - 1, fmt, ##__VA_ARGS__); \
            error_list.push_back(tmp_buf); \
        }

        if(g_DisplayControlWindow) {
            ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_Once);
            ImGui::Begin("Control window", &g_DisplayControlWindow);

            ImGui::TextWrapped("Space discretized, dimensions: %ld", g_QuantumSimulator.GetDimensions());
            ImGui::TextWrapped("Time discretized, current iteration: %ld", g_QuantumSimulator.GetIteration());

            ImGui::Separator();

            if(ImGui::Button("Reset to default")) {
                _SIM_RESET_DEFAULT;
            }
            if(ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Restore default settings and restart simulation");
            }

            ImGui::Separator();

            ImGui::InputDouble("hslash", &g_EditHslash);
            if(ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Value of Planck's reduced constant");
            }
            if(g_EditHslash != g_QuantumSimulator.GetHslash()) {
                g_QuantumSimulator.UpdateHslash(g_EditHslash);
                _SIM_RESET;
            }

            ImGui::InputDouble("m", &g_EditMass);
            if(ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Value of particle's mass");
            }
            if(g_EditMass != g_QuantumSimulator.GetMass()) {
                g_QuantumSimulator.UpdateMass(g_EditMass);
                _SIM_RESET;
            }

            ImGui::Separator();

            ImGui::InputDouble("x0", &g_EditSpaceStart);
            if(ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Start of the simulated space interval");
            }
            if(g_EditSpaceStart != g_QuantumSimulator.GetSpaceStart()) {
                g_QuantumSimulator.UpdateSpaceStart(g_EditSpaceStart);
                _SIM_RESET;
            }

            ImGui::InputDouble("xf", &g_EditSpaceEnd);
            if(ImGui::IsItemHovered()) {
                ImGui::SetTooltip("End of the simulated space interval");
            }
            if(g_EditSpaceEnd != g_QuantumSimulator.GetSpaceEnd()) {
                g_QuantumSimulator.UpdateSpaceEnd(g_EditSpaceEnd);
                _SIM_RESET;
            }

            ImGui::InputDouble("dx", &g_EditSpaceStep);
            if(ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Space step (size used for space discretizing)");
            }
            if(g_EditSpaceStep != g_QuantumSimulator.GetSpaceStep()) {
                g_QuantumSimulator.UpdateSpaceStep(g_EditSpaceStep);
                _SIM_RESET;
            }

            ImGui::Separator();

            ImGui::InputDouble("t0", &g_EditTimeStart);
            if(ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Initial time value (time corresponding to first iteration)");
            }
            if(g_EditTimeStart != g_QuantumSimulator.GetTimeStart()) {
                g_QuantumSimulator.UpdateTimeStart(g_EditTimeStart);
                _SIM_RESET;
            }
            
            ImGui::InputDouble("dt", &g_EditTimeStep);
            if(ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Time step (size used for time discretizing)");
            }
            if(g_EditTimeStep != g_QuantumSimulator.GetTimeStep()) {
                g_QuantumSimulator.UpdateTimeStep(g_EditTimeStep);
                _SIM_RESET;
            }

            ImGui::Separator();

            ImGui::Checkbox("Auto-start", &g_AutoStart);
            if(ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Automatically start running the simulation after anything is changed");
            }

            ImGui::Checkbox(g_Running ? "Running" : "Paused", &g_Running);
            if(ImGui::IsItemHovered()) {
                ImGui::SetTooltip(g_Running ? "Simulation is running, click to pause" : "Simulation is paused, click to resume");
            }

            if(ImGui::Button("Restart")) {
                _SIM_RESET;
            }
            if(ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Restart simulation");
            }

            ImGui::End();
        }

        bool psi0_src_changed = false;
        bool v_src_changed = false;

        if(g_DisplaySourceWindow) {
            ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_Once);
            ImGui::Begin("Source editor", &g_DisplaySourceWindow, ImGuiWindowFlags_MenuBar);

            if(ImGui::BeginMenuBar()) {
                if(ImGui::BeginMenu("Ψ0 demos")) {
                    for(size_t i = 0; i < Psi0DemoSourceCount; i++) {
                        if(ImGui::MenuItem(Psi0DemoSourceNames[i])) {
                            strcpy(g_EditPsi0Source, Psi0DemoSources[i]);
                            g_QuantumSimulator.UpdatePsi0Source(g_EditPsi0Source);
                            psi0_src_changed = true;
                            _SIM_RESET;
                        }
                    }

                    ImGui::EndMenu();
                }

                if(ImGui::BeginMenu("V demos")) {
                    for(size_t i = 0; i < VDemoSourceCount; i++) {
                        if(ImGui::MenuItem(VDemoSourceNames[i])) {
                            strcpy(g_EditVSource, VDemoSources[i]);
                            g_QuantumSimulator.UpdateVSource(g_EditVSource);
                            v_src_changed = true;
                            _SIM_RESET;
                        }
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndMenuBar();
            }

            if(ImGui::BeginTabBar("SrcTab")) {
                if(ImGui::BeginTabItem("Coding notes")) {
                    _DO_WITH_TEXT_COLOR(NoteColor, {
                        ImGui::TextWrapped(SourceGlobalsNoticeText);
                        ImGui::TextWrapped(SourceFunctionsNoticeText);
                        ImGui::TextWrapped(SourceLibrariesNoticeText);
                        ImGui::TextWrapped(SourceEvaluationNoticeText);
                    });

                    ImGui::EndTabItem();
                }
                
                if(ImGui::BeginTabItem("Ψ0 source")) {
                    ImGui::InputTextMultiline("##Psi0Src", g_EditPsi0Source, sizeof(g_EditPsi0Source), ImGui::GetContentRegionAvail(), ImGuiInputTextFlags_AllowTabInput);
                    if(!g_QuantumSimulator.ComparePsi0Source(g_EditPsi0Source)) {
                        g_QuantumSimulator.UpdatePsi0Source(g_EditPsi0Source);
                        psi0_src_changed = true;
                        _SIM_RESET;
                    }

                    ImGui::EndTabItem();
                }

                if(ImGui::BeginTabItem("V source")) {
                    ImGui::InputTextMultiline("##VSrc", g_EditVSource, sizeof(g_EditVSource), ImGui::GetContentRegionAvail(), ImGuiInputTextFlags_AllowTabInput);
                    if(!g_QuantumSimulator.CompareVSource(g_EditVSource)) {
                        g_QuantumSimulator.UpdateVSource(g_EditVSource);
                        v_src_changed = true;
                        _SIM_RESET;
                    }

                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }            

            ImGui::End();
        }

        if(!g_QuantumSimulator.IsPsi0SourceEvaluated() || psi0_src_changed) {
            EvaluateJsSimulationVariables();
            const auto psi0_rc = TryEvaluate(g_EditPsi0Source);
            g_QuantumSimulator.NotifyPsi0SourceEvaluated(JS_RC_SUCCEEDED(psi0_rc));
        }
        if(!g_QuantumSimulator.IsPsi0SourceOk()) {
            _PUSH_ERROR_FMT("Ψ0 source error");
        }

        if(!g_QuantumSimulator.IsVSourceEvaluated() || v_src_changed) {
            EvaluateJsSimulationVariables();
            const auto v_rc = TryEvaluate(g_EditVSource);
            g_QuantumSimulator.NotifyVSourceEvaluated(JS_RC_SUCCEEDED(v_rc));
        }
        if(!g_QuantumSimulator.IsVSourceOk()) {
            _PUSH_ERROR_FMT("V source error");
        }
        
        // Check for invalid states
        if(g_QuantumSimulator.GetDimensions() == 0) {
            _PUSH_ERROR_FMT("x0 must not equal to xf");
        }
        else if(g_QuantumSimulator.GetSpaceStart() > g_QuantumSimulator.GetSpaceEnd()) {
            _PUSH_ERROR_FMT("x0 must be smaller than xf");
        }
        else if(g_QuantumSimulator.GetSpaceStep() <= 0) {
            _PUSH_ERROR_FMT("space step must be strictly positive");
        }
        else if(g_QuantumSimulator.GetDimensions() > MaxSupportedDimensions) {
            _PUSH_ERROR_FMT("too many discretization dimensions (%ld > limit=%ld), too small space step and/or too big space start/end interval", g_QuantumSimulator.GetDimensions(), MaxSupportedDimensions);
        }
        else if(g_QuantumSimulator.GetTimeStep() <= 0) {
            _PUSH_ERROR_FMT("time step must be strictly positive");
        }

        if(error_list.empty()) {
            if(g_QuantumSimulator.GetIteration() == 0) {
                if(!g_QuantumSimulator.ComputeNextIteration()) {
                    if(!g_QuantumSimulator.IsPsi0SourceOk()) {
                        _PUSH_ERROR_FMT("error in initial Ψ0 invocation");
                    }
                    if(!g_QuantumSimulator.IsVSourceOk()) {
                        _PUSH_ERROR_FMT("error in initial V invocation");
                    }
                }
            }
            else {
                if(was_reset || (g_Running && (g_QuantumSimulator.GetIteration() < MaxSupportedIterations))) {
                    if(!g_QuantumSimulator.ComputeNextIteration()) {
                        if(!g_QuantumSimulator.IsPsi0SourceOk()) {
                            _PUSH_ERROR_FMT("error in psi0 invocation");
                        }
                        if(!g_QuantumSimulator.IsVSourceOk()) {
                            _PUSH_ERROR_FMT("error in V invocation");
                        }
                    }
                }
            }
        }

        const auto sim_initialized = error_list.empty() && (g_QuantumSimulator.GetIteration() > 0);

        if(!error_list.empty()) {
            ImGui::SetNextWindowSize(ImVec2(800, 200), ImGuiCond_Once);
            ImGui::Begin("Simulation errors", nullptr, ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNavFocus);

            ImGui::TextWrapped("Cannot run the simulation because of the following errors:");

            _DO_WITH_TEXT_COLOR(ErrorColor, {
                for(const auto &error: error_list) {
                    ImGui::Separator();
                    ImGui::TextWrapped("ERROR: %s", error.c_str());
                }
            });

            ImGui::End();
        }

        if(g_DisplaySpacePlotWindow) {
            ImGui::SetNextWindowSize(ImVec2(800, 430), ImGuiCond_Once);
            ImGui::Begin("Space plot", &g_DisplaySpacePlotWindow);

            if(sim_initialized) {
                _DO_WITH_TEXT_COLOR(NoteColor, {
                    ImGui::TextWrapped("NOTE: the (x0, xf) space region limit is equivalent to V being infinite outside the studied region");
                });

                ImGui::Separator();
                
                ImGui::TextWrapped("Ψ norm: %f", g_QuantumSimulator.GetCurrentPsiNorm());

                if(ImPlot::BeginPlot("Space evolution")) {
                    ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_None, ImPlotAxisFlags_None);
                    ImPlot::SetupAxesLimits(g_QuantumSimulator.GetSpaceStart(), g_QuantumSimulator.GetSpaceEnd(), 0, g_QuantumSimulator.GetCurrentPsiSquareNormDiscreteVector().maxCoeff());

                    ImPlot::PlotLine("|Ψ|²", g_QuantumSimulator.GetXDiscreteVector().data(), g_QuantumSimulator.GetCurrentPsiSquareNormDiscreteVector().data(), g_QuantumSimulator.GetDimensions());
                    ImPlot::PlotLine("V", g_QuantumSimulator.GetXDiscreteVector().data(), g_QuantumSimulator.GetCurrentVDiscreteVector().data(), g_QuantumSimulator.GetDimensions());

                    ImPlot::EndPlot();
                }
            }

            ImGui::End();
        }

        if(g_DisplaySpaceOpsPlotWindow) {
            ImGui::SetNextWindowSize(ImVec2(800, 435), ImGuiCond_Once);
            ImGui::Begin("Space operator plot", &g_DisplaySpaceOpsPlotWindow);

            if(sim_initialized) {
                ImGui::TextWrapped("x: %f", g_QuantumSimulator.GetCurrentXEstimateValue());
                if(ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Estimated value of position (x operator)");
                }

                ImGui::TextWrapped("x²: %f", g_QuantumSimulator.GetCurrentXSquaredEstimateValue());
                if(ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Estimated value of x² operator");
                }

                ImGui::TextWrapped("Δx: %f", g_QuantumSimulator.GetCurrentDeltaXValue());
                if(ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Position uncertainty");
                }

                if(ImPlot::BeginPlot("Space operator evolution")) {
                    ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_None, ImPlotAxisFlags_AutoFit);
                    ImPlot::SetupAxesLimits(0, g_QuantumSimulator.GetIteration(), 0, 0);

                    ImPlot::PlotLine("x", g_QuantumSimulator.GetIterationRecord().data(), g_QuantumSimulator.GetXEstimateRecord().data(), g_QuantumSimulator.GetRecordSize());
                    ImPlot::PlotLine("x²", g_QuantumSimulator.GetIterationRecord().data(), g_QuantumSimulator.GetXSquaredEstimateRecord().data(), g_QuantumSimulator.GetRecordSize());
                    ImPlot::PlotLine("Δx", g_QuantumSimulator.GetIterationRecord().data(), g_QuantumSimulator.GetDeltaXRecord().data(), g_QuantumSimulator.GetRecordSize());

                    ImPlot::EndPlot();
                }
            }

            ImGui::End();
        }

        if(g_DisplayMomentumOpsPlotWindow) {
            ImGui::SetNextWindowSize(ImVec2(800, 435), ImGuiCond_Once);
            ImGui::Begin("Momentum operator plot", &g_DisplayMomentumOpsPlotWindow);

            if(sim_initialized) {
                ImGui::TextWrapped("p: %f", g_QuantumSimulator.GetCurrentPEstimateValue());
                if(ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Estimated value of linear momentum (p operator)");
                }

                ImGui::TextWrapped("p²: %f", g_QuantumSimulator.GetCurrentPSquaredEstimateValue());
                if(ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Estimated value of p² operator");
                }

                ImGui::TextWrapped("Δp: %f", g_QuantumSimulator.GetCurrentDeltaPValue());
                if(ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Momentum uncertainty");
                }

                if(ImPlot::BeginPlot("Momentum operator evolution")) {
                    ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_None, ImPlotAxisFlags_AutoFit);
                    ImPlot::SetupAxesLimits(0, g_QuantumSimulator.GetIteration(), 0, 0);

                    ImPlot::PlotLine("p", g_QuantumSimulator.GetIterationRecord().data(), g_QuantumSimulator.GetPEstimateRecord().data(), g_QuantumSimulator.GetRecordSize());
                    ImPlot::PlotLine("p²", g_QuantumSimulator.GetIterationRecord().data(), g_QuantumSimulator.GetPSquaredEstimateRecord().data(), g_QuantumSimulator.GetRecordSize());
                    ImPlot::PlotLine("Δp", g_QuantumSimulator.GetIterationRecord().data(), g_QuantumSimulator.GetDeltaPRecord().data(), g_QuantumSimulator.GetRecordSize());

                    ImPlot::EndPlot();
                }
            }

            ImGui::End();
        }

        if(g_DisplayUncertaintyPlotWindow) {
            ImGui::SetNextWindowSize(ImVec2(800, 380), ImGuiCond_Once);
            ImGui::Begin("Uncertainty plot", &g_DisplayUncertaintyPlotWindow);

            if(sim_initialized) {
                ImGui::TextWrapped("hslash/2 = %f", g_QuantumSimulator.GetHslash() / 2);
                if(ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Minimum position/momentum uncertainty (per Heisenberg's uncertainty principle)");
                }
                
                ImGui::TextWrapped("ΔxΔp: %f", g_QuantumSimulator.GetCurrentDeltaProductValue());
                if(ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Position/momentum uncertainty");
                }

                if(ImPlot::BeginPlot("Uncertainty evolution")) {
                    ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_None, ImPlotAxisFlags_AutoFit);
                    ImPlot::SetupAxesLimits(0, g_QuantumSimulator.GetIteration(), 0, 0);

                    ImPlot::PlotLine("ΔxΔp", g_QuantumSimulator.GetIterationRecord().data(), g_QuantumSimulator.GetDeltaProductRecord().data(), g_QuantumSimulator.GetRecordSize());

                    ImPlot::EndPlot();
                }
            }

            ImGui::End();
        }

        if(g_DisplayEnergyPlotWindow) {
            ImGui::SetNextWindowSize(ImVec2(800, 380), ImGuiCond_Once);
            ImGui::Begin("Energy plot", &g_DisplayEnergyPlotWindow);

            if(sim_initialized) {
                ImGui::TextWrapped("E: %f", g_QuantumSimulator.GetCurrentEnergyEstimateValue());
                if(ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Estimated energy value (H operator)");
                }

                if(ImPlot::BeginPlot("Energy evolution")) {
                    ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_None, ImPlotAxisFlags_AutoFit);
                    ImPlot::SetupAxesLimits(0, g_QuantumSimulator.GetIteration(), 0, 0);

                    ImPlot::PlotLine("E", g_QuantumSimulator.GetIterationRecord().data(), g_QuantumSimulator.GetEnergyEstimateRecord().data(), g_QuantumSimulator.GetRecordSize());

                    ImPlot::EndPlot();
                }
            }

            ImGui::End();
        }

        ImGui::SetNextWindowSize(ImVec2(600, 250), ImGuiCond_Once);
        if(g_DisplayAboutWindow) {
            ImGui::Begin("About quantize", &g_DisplayAboutWindow);

            if(ImGui::Button("GitHub")) {
                OpenUrl("https://github.com/XorTroll/quantize");
            }
            ImGui::SameLine();

            _DO_WITH_TEXT_COLOR(ImVec4(0.5f, 0.0f, 1.0f, 1.0f), {
                ImGui::TextWrapped("Feel free to submit bugs or suggestions!");
            });

            ImGui::TextWrapped("C++20 (%ld), clang v" __clang_version__, __cplusplus);

            ImGui::Separator();

            const auto em_ver = (char*)emscripten_get_compiler_setting("EMSCRIPTEN_VERSION");
            if(ImGui::Button("emscripten")) {
                OpenUrl("https://emscripten.org/");
            }
            ImGui::SameLine();
            ImGui::TextWrapped("v%s", em_ver);

            ImGui::Separator();

            if(ImGui::Button("ImGui")) {
                OpenUrl("https://github.com/ocornut/imgui");
            }
            ImGui::SameLine();
            ImGui::TextWrapped("v" IMGUI_VERSION " (%d)", IMGUI_VERSION_NUM);

            ImGui::Separator();

            if(ImGui::Button("ImPlot")) {
                OpenUrl("https://github.com/epezent/implot");
            }
            ImGui::SameLine();
            ImGui::TextWrapped("v" IMPLOT_VERSION);

            ImGui::Separator();

            if(ImGui::Button("Eigen")) {
                OpenUrl("https://eigen.tuxfamily.org/index.php?title=Main_Page");
            }
            ImGui::SameLine();
            ImGui::TextWrapped("v%d.%d.%d", EIGEN_WORLD_VERSION, EIGEN_MAJOR_VERSION, EIGEN_MINOR_VERSION);

            ImGui::Separator();

            if(ImGui::Button("nlohmann-json")) {
                OpenUrl("https://json.nlohmann.me");
            }
            ImGui::SameLine();
            ImGui::TextWrapped("v%d.%d.%d", NLOHMANN_JSON_VERSION_MAJOR, NLOHMANN_JSON_VERSION_MINOR, NLOHMANN_JSON_VERSION_PATCH);
            
            if(ImGui::Button("math.js")) {
                OpenUrl("https://mathjs.org/docs/index.html");
            }
            ImGui::SameLine();
            ImGui::TextWrapped("v%s", GetMathJsVersion());

            ImGui::Separator();

            if(ImGui::Button("FiraCode")) {
                OpenUrl("https://github.com/tonsky/FiraCode");
            }

            ImGui::End();
        }

        ImGui::Render();

        glfwMakeContextCurrent(g_Window);
        int display_w, display_h;
        glfwGetFramebufferSize(g_Window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(ClearColor.x, ClearColor.y, ClearColor.z, ClearColor.w);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwMakeContextCurrent(g_Window);
    }

    int InitializeGlfw() {
        if(glfwInit() != GLFW_TRUE) {
            ShowError("Failed to initialize GLFW!");
            return 1;
        }

        // We don't want the old OpenGL
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        // Open a window and create its OpenGL context
        g_Window = glfwCreateWindow(g_Width, g_Height, "WebGui Demo", nullptr, nullptr);
        if(g_Window == nullptr) {
            ShowError("Failed to open GLFW window!");
            glfwTerminate();
            return -1;
        }

        // Initialize GLFW
        glfwMakeContextCurrent(g_Window);
        return 0;
    }

    constexpr ImWchar ImGuiGlyphRanges[] = {
        0x0020, 0x00FF, // Basic Latin + Latin Supplement
        0x0370, 0x03FF, // Greek and Coptic
        0x2100, 0x214F, // Letterlike Symbols (https://codepoints.net/letterlike_symbols)
        0x0000
    };

    int InitializeImGui() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForOpenGL(g_Window, true);
        ImGui_ImplOpenGL3_Init();
        ImPlot::CreateContext();

        ImGui::StyleColorsDark();

        auto &io = ImGui::GetIO();
        io.Fonts->AddFontFromFileTTF("assets/FiraCode-Regular.ttf", 18.0f, nullptr, ImGuiGlyphRanges);
        io.Fonts->AddFontFromFileTTF("assets/FiraCode-Regular.ttf", 26.0f, nullptr, ImGuiGlyphRanges);
        io.Fonts->AddFontDefault();

        ResizeCanvas();

        return 0;
    }

    #define _RES_TRY(expr) { \
        const auto tmp_res = (expr); \
        if(tmp_res != 0) { \
            return tmp_res; \
        } \
    }

    int Initialize() {
        g_Width = GetCanvasWidth();
        g_Height = GetCanvasHeight();

        _RES_TRY(InitializeGlfw());
        _RES_TRY(InitializeImGui());
        InitializeJsExports();

        ResetSimulationToDefault();

        return 0;
    }

    void Finalize() {
        glfwTerminate();
    }

}

extern "C" int main(int argc, char **argv) {
    if(Initialize() != 0) {
        ShowError("Failed to initialize!");
        return 1;
    }

    emscripten_set_main_loop(MainLoop, 0, 1);

    Finalize();
    return 0;
}
