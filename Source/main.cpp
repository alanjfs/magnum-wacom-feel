// (Optional) Magnum prefers to have its imgui.h included first
#include <Magnum/ImGuiIntegration/Context.hpp>

#include <string>
#include <unordered_map>

#include <Magnum/Math/Color.h>
#include <Magnum/Math/Vector.h>
#include <Magnum/Magnum.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/GL/Version.h>
#include <Magnum/Platform/GlfwApplication.h>
#include <Corrade/Utility/Resource.h>

#include <entt/entity/registry.hpp>
#include <imgui_internal.h> // DockBuilderDockWindow

using namespace Magnum;
using namespace Math::Literals;

#include "Theme.inl"
#include "Wacom.h"

entt::registry Registry;


class Application : public Platform::Application {
public:
    explicit Application(const Arguments& arguments);
    void drawEvent() override;
    void drawCentralWidget();

private:
    auto dpiScaling() const -> Vector2;
    void viewportEvent(ViewportEvent& event) override;

    void keyPressEvent(KeyEvent& event) override;
    void keyReleaseEvent(KeyEvent& event) override;

    void mousePressEvent(MouseEvent& event) override;
    void mouseReleaseEvent(MouseEvent& event) override;
    void mouseMoveEvent(MouseMoveEvent& event) override;
    void mouseScrollEvent(MouseScrollEvent& event) override;
    void textInputEvent(TextInputEvent& event) override;

    ImGuiIntegration::Context _imgui{ NoCreate };
    Vector2                   _dpiScaling { 1.0f, 1.0f };
    Wacom::Touch              _wacomTouch;

    enum Mode {
        Draw = 0, Monitor
    };

    int mode { Monitor };

    struct Line {
        std::vector<ImVec2> positions;
        float radius { 1.0f };
        ImColor color;
        bool fill { false };
    };

    std::vector<Line> lines;
    bool fill { false };
};


Application::Application(const Arguments& arguments) : Platform::Application{
    arguments,
    Configuration{}.setTitle("Wacom Test")
                       .setSize({1600, 900})
                       .setWindowFlags(Configuration::WindowFlag::Resizable),
    GLConfiguration{}
        .setVersion(GL::Version::GL420)
        .setSampleCount(2)
} {
    // Use virtual scale, rather than the default physical
    glfwGetWindowContentScale(this->window(), &_dpiScaling.x(), &_dpiScaling.y());

    // Center window on primary monitor
    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    glfwSetWindowPos(this->window(),
        (mode->width / 2) - (windowSize().x() / 2),
        (mode->height / 2) - (windowSize().y() / 2)
    );

    _imgui = ImGuiIntegration::Context(
        Vector2{ windowSize() } / dpiScaling(),
        windowSize(), framebufferSize()
    );

    auto& io = ImGui::GetIO();
    io.Fonts->Clear();

    ImFontConfig fontConfig;
    fontConfig.FontDataOwnedByAtlas = false;

    Utility::Resource rs{"data"};
    Containers::ArrayView<const char> font = rs.getRaw("OpenSans.ttf");
    io.Fonts->AddFontFromMemoryTTF(const_cast<char*>(font.data()), font.size(), 16.0f * dpiScaling().x(), &fontConfig);
    io.Fonts->AddFontFromMemoryTTF(const_cast<char*>(font.data()), font.size(), 24.0f * dpiScaling().x(), &fontConfig);

    // Refresh fonts
    _imgui.relayout(
        Vector2{ windowSize() } / dpiScaling(),
        windowSize(), framebufferSize()
    );

    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.ConfigWindowsResizeFromEdges = true;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;    // Enable Docking
    io.ConfigDockingWithShift = true;

    Theme();

    /* Set up proper blending to be used by ImGui */
    GL::Renderer::setBlendEquation(GL::Renderer::BlendEquation::Add,
                                   GL::Renderer::BlendEquation::Add);
    GL::Renderer::setBlendFunction(GL::Renderer::BlendFunction::SourceAlpha,
                                   GL::Renderer::BlendFunction::OneMinusSourceAlpha);

    GL::Renderer::enable(GL::Renderer::Feature::Blending);
    GL::Renderer::enable(GL::Renderer::Feature::ScissorTest);
    GL::Renderer::disable(GL::Renderer::Feature::FaceCulling);
    GL::Renderer::disable(GL::Renderer::Feature::DepthTest);

    this->setSwapInterval(1);  // VSync

    if (_wacomTouch.init()) {
        Debug() << "Successfully initialised the Wacom SDK.\n";
    } else {
        Debug() << "Couldn't initialise the Wacom SDK";
        abort();
    }

    _wacomTouch.printAttachedDevices();
}


auto Application::dpiScaling() const -> Vector2 { return _dpiScaling; }



void Application::drawCentralWidget() {
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking
                                 | ImGuiWindowFlags_NoTitleBar
                                 | ImGuiWindowFlags_NoCollapse
                                 | ImGuiWindowFlags_NoResize
                                 | ImGuiWindowFlags_NoMove
                                 | ImGuiWindowFlags_NoBringToFrontOnFocus
                                 | ImGuiWindowFlags_NoNavFocus;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    // This is basically the background window that contains all the dockable windows
    ImGui::Begin("InvisibleWindow", nullptr, windowFlags);
    ImGui::PopStyleVar(3);

    ImGuiID dockSpaceId = ImGui::GetID("InvisibleWindowDockSpace");

    if(!ImGui::DockBuilderGetNode(dockSpaceId)) {
        ImGui::DockBuilderAddNode(dockSpaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockSpaceId, viewport->Size);

        ImGuiID center = dockSpaceId;
        ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.25f, nullptr, &center);

        ImGui::DockBuilderDockWindow("Canvas", left);
        ImGui::DockBuilderFinish(center);
    }

    ImGui::DockSpace(dockSpaceId, ImVec2(0.0f, 0.0f));
    ImGui::End();
}


void Application::drawEvent() {
    GL::defaultFramebuffer.clear(GL::FramebufferClear::Color);
    _imgui.newFrame();

         if ( ImGui::GetIO().WantTextInput && !isTextInputActive()) startTextInput();
    else if (!ImGui::GetIO().WantTextInput &&  isTextInputActive()) stopTextInput();

    drawCentralWidget();

    auto GetColor = [](int id, float opacity = 1.0f) -> ImColor {
        auto col = ImColor::HSV(float(id) / 10.0f, 0.5f, 1.0f);
        col.Value.w = opacity;
        return col;
    };

    auto MonitorMode = [&]() {
        const auto size = ImGui::GetWindowSize();
        static float speed { 0.1f };
        ImGui::BeginChild("Options", ImVec2{ 300.0f, 400.0f }, false);
        {
            ImGui::SliderFloat("Fade Velocity", &speed, 0.0f, 1.0f, "", 3.0f);
        }
        ImGui::EndChild();

        using FingerEvents = std::unordered_map<Wacom::FingerId, std::vector<Wacom::TouchEvent>>;
        using FingerOpacities = std::unordered_map<Wacom::FingerId, float>;

        static FingerEvents events;
        static FingerOpacities opacities;

        for (const auto [id, finger] : _wacomTouch.poll()) {
            if (!finger.confidence) continue;

            if (finger.state == Wacom::TouchState::Down) {
                if (events.count(finger.fingerId)) events.erase(finger.fingerId);
            }

            events[finger.fingerId].push_back(finger);
            opacities[finger.fingerId] = 1.0f;
        }

        auto& painter = *ImGui::GetForegroundDrawList();
        for (auto [id, events] : events) {
            painter.PathClear();
            const auto col = GetColor(id, opacities.at(id));
            for (auto finger : events) {
                painter.PathLineTo(ImVec2{ finger.x * size.x, finger.y * size.y });
            }
            painter.PathStroke(col, false);

            auto finger = events.back();
            const auto radius = (finger.width + finger.height) * 50.0f;
            const auto pos = ImVec2{ finger.x * size.x, finger.y * size.y };
            painter.AddCircle(pos, radius, col);
            painter.AddText({ pos.x + 10.0f, pos.y - 10.0f }, col, std::to_string(finger.fingerId).c_str());
        }

        std::vector<int> erase;
        for (auto& [id, opacity] : opacities) {
            opacity -= opacity * speed;

            if (opacity < 0.0f) {
                erase.push_back(id);
            }
        }

        for (auto id : erase) {
            opacities.erase(id);
            events.erase(id);
        }
    };

    auto DrawMode = [&]() {
        const auto size = ImGui::GetWindowSize();
        ImGui::BeginChild("Options", ImVec2{ 300.0f, 400.0f }, false);
        {
            ImGui::Checkbox("Fill", &fill);
        }
        ImGui::EndChild();

        auto fingers = _wacomTouch.poll();
        static bool drawingInProgress { false };
        std::string status { "" };
        auto& painter = *ImGui::GetForegroundDrawList();

        if (fingers.count(0)) {
            auto finger = fingers.at(0);
            const auto radius = (finger.width + finger.height) * 50.0f;
            const auto col = GetColor(finger.fingerId);
            const auto pos = ImVec2{ finger.x * size.x, finger.y * size.y };
            painter.AddCircle(pos, radius, col);

            status = "Cursor";

            if (fingers.count(1)) {
                status = "Draw";

                if (!drawingInProgress) {
                    auto finger = fingers.at(0);
                    const auto radius = (finger.width + finger.height) * 10.0f;
                    lines.push_back({ {}, radius, GetColor(lines.size()), fill });
                }

                drawingInProgress = true;
            } else {
                drawingInProgress = false;
            }

            if (fingers.count(2)) {
                status = "Size";
                drawingInProgress = false;
            }
        }

        if (drawingInProgress) {
            auto finger = fingers.at(0);
            const auto pos = ImVec2{ finger.x * size.x, finger.y * size.y };
            auto& line = lines.back();
            line.positions.push_back(pos);
        }

        ImFont* font = ImGui::GetIO().Fonts->Fonts[1];
        // auto* font = ImGui::GetFont();
        const ImVec2 center = { ImGui::GetWindowWidth() * 0.5f, ImGui::GetWindowHeight() * 0.5f };
        painter.AddText(font, 24.0f, center, ImColor::HSV(0.0f, 0.0f, 1.0f), status.c_str());

        for (auto line : lines) {
            painter.PathClear();
            for (auto pos : line.positions) painter.PathLineTo(ImVec2{ pos.x, pos.y });
            if (line.fill) painter.PathFillConvex(line.color);
            else           painter.PathStroke(line.color, false, line.radius);
        }
    };

    ImGui::Begin("Canvas", nullptr);
    {
        bool temp = this->mode == Draw;
        if (ImGui::Checkbox("Draw Mode", &temp)) this->mode = mode == Monitor ? Draw : Monitor;
        if (this->mode == Monitor) MonitorMode();
        if (this->mode == Draw) DrawMode();
    }
    ImGui::End();

    _imgui.drawFrame();
    swapBuffers();
    redraw();
}


void Application::viewportEvent(ViewportEvent& event) {
    GL::defaultFramebuffer.setViewport({{}, event.framebufferSize()});

    _imgui.relayout(Vector2{ event.windowSize() } / dpiScaling(),
        event.windowSize(), event.framebufferSize());
}


void Application::keyPressEvent(KeyEvent& event) {
    if (event.key() == KeyEvent::Key::Esc)          this->exit();
    if (event.key() == KeyEvent::Key::F)            this->fill ^= true;
    if (event.key() == KeyEvent::Key::Space)        {
        lines.clear();
        this->mode = mode == Monitor ? Draw : Monitor;
    }
    if(_imgui.handleKeyPressEvent(event)) return;
}


void Application::keyReleaseEvent(KeyEvent& event) {
    if(_imgui.handleKeyReleaseEvent(event)) return;
}


void Application::mousePressEvent(MouseEvent& event) {
    if (_imgui.handleMousePressEvent(event)) return;
}


void Application::mouseMoveEvent(MouseMoveEvent& event) {
    if (_imgui.handleMouseMoveEvent(event)) return;
}


void Application::mouseReleaseEvent(MouseEvent& event) {
    if (_imgui.handleMouseReleaseEvent(event)) return;
}

void Application::mouseScrollEvent(MouseScrollEvent& event) {
    if(_imgui.handleMouseScrollEvent(event)) {
        /* Prevent scrolling the page */
        event.setAccepted();
        return;
    }
}

void Application::textInputEvent(TextInputEvent& event) {
    if(_imgui.handleTextInputEvent(event)) return;
}


MAGNUM_APPLICATION_MAIN(Application)