// (Optional) Magnum prefers to have its imgui.h included first
#include <Magnum/ImGuiIntegration/Context.hpp>

#include <string>
#include <unordered_map>

#include <Magnum/Math/Color.h>
#include <Magnum/Math/Vector.h>
#include <Magnum/Magnum.h>

#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/Platform/GlfwApplication.h>

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
};


Application::Application(const Arguments& arguments): Platform::Application{arguments,
    Configuration{}.setTitle("Wacom Example Application")
                   .setSize({1600, 900})
                   .setWindowFlags(Configuration::WindowFlag::Resizable)}
{
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

    ImGui::GetIO().Fonts->Clear();
    ImGui::GetIO().Fonts->AddFontFromFileTTF("OpenSans-Regular.ttf", 16.0f * dpiScaling().x());

    // Refresh fonts
    _imgui.relayout(
        Vector2{ windowSize() } / dpiScaling(),
        windowSize(), framebufferSize()
    );

    // Required, else you can't interact with events in the editor
    ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;

    // Optional
    ImGui::GetIO().ConfigWindowsResizeFromEdges = true;
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;    // Enable Docking
    ImGui::GetIO().ConfigDockingWithShift = true;

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

    using FingerEvents = std::unordered_map<Wacom::FingerId, std::vector<Wacom::TouchEvent>>;
    using FingerOpacities = std::unordered_map<Wacom::FingerId, float>;

    static FingerEvents events;
    static FingerOpacities opacities;

    auto GetColor = [](int id, float opacity = 1.0f) -> ImColor {
        auto col = ImColor::HSV(float(id) / 10.0f, 0.5f, 1.0f);
        col.Value.w = opacity;
        return col;
    };

    static float speed { 0.1f };
    static bool mode { false };

    ImGui::Begin("Canvas", nullptr);
    {
        ImGui::VSliderFloat("Fade Velocity", ImVec2{ 40.0f, 400.0f }, &speed, 0.0f, 1.0f, "", 3.0f);
        ImGui::Checkbox("Draw Mode", &mode);

        for (const auto [id, finger] : _wacomTouch.poll()) {
            if (!finger.confidence) continue;

            if (finger.state == Wacom::TouchState::Down) {
                if (events.count(finger.fingerId)) events.erase(finger.fingerId);
            }

            events[finger.fingerId].push_back(finger);
            opacities[finger.fingerId] = 1.0f;
        }

        auto& painter = *ImGui::GetWindowDrawList();
        const auto size = ImGui::GetWindowSize();
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
            painter.AddText({ pos.x + 10.0f, pos.y + 10.0f }, col, std::to_string(finger.sensitivity).c_str());
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