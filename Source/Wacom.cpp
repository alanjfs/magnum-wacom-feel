#include <iostream>
#include "Wacom.h"


namespace Wacom {


static void OnAttached(WacomMTCapability deviceInfo, void *userInfo) {
    Touch* tablet = static_cast<Touch*>(userInfo);
    tablet->_deviceAttached(deviceInfo);
}


static void OnDetached(int deviceID, void *userInfo) {
    Touch* tablet = static_cast<Touch*>(userInfo);
    tablet->_deviceDetached(deviceID);
}


static int OnFinger(WacomMTFingerCollection *fingerPacket, void *userInfo) {
    Touch* tablet = static_cast<Touch*>(userInfo);
    tablet->_fingerCallBack(fingerPacket);
    return 0;
}


bool Touch::init() {
    bool wasInitialised = false;

    WacomMTError err = WacomMTInitialize(WACOM_MULTI_TOUCH_API_VERSION);

    if (err == WMTErrorSuccess) {
        /**
         * @brief Listen for device connect/disconnect.
         *
         * Note that the attach callback will be called for each connected device
         * immediately after the callback is registered.
         *
         */
        WacomMTRegisterAttachCallback(OnAttached, this);
        WacomMTRegisterDetachCallback(OnDetached, this);

        int deviceIDs[MAX_ATTACHED_DEVICES]  = {};
        int deviceCount    = 0;

        // Add a viewer for each device's data
        deviceCount = WacomMTGetAttachedDeviceIDs(deviceIDs, sizeof(deviceIDs));

        if (deviceCount > MAX_ATTACHED_DEVICES) {
            // With a number as big as 30, this will never actually happen.
            std::cerr << "More tablets connected than would fit in "
                      << "the supplied buffer.\n";
        }
        else {
            for(int ii = 0; ii < deviceCount; ii++) {
                WacomMTRegisterFingerReadCallback(
                    deviceIDs[ii],          // deviceID
                    nullptr,                // hitRect
                    WMTProcessingModeNone,  // mode
                    OnFinger,               // fingerCallback
                    this                    // userData
                );
            }

            wasInitialised = true;
        }
    }

    return wasInitialised;
}


void Touch::_deviceAttached(WacomMTCapability deviceInfo) {
}


void Touch::_deviceDetached(int deviceID) {
}


void Touch::_fingerCallBack(WacomMTFingerCollection *fingerPacket) {
    for(int fingerIndex = 0; fingerIndex < fingerPacket->FingerCount; fingerIndex++)
    {
        WacomMTFinger* finger = &fingerPacket->Fingers[fingerIndex];

        TouchEvent event{};
        event.fingerId     = finger->FingerID;
        event.x            = finger->X;
        event.y            = finger->Y;            
        event.width        = finger->Width;
        event.height       = finger->Height;
        event.orientation  = finger->Orientation;
        event.confidence   = finger->Confidence;
        event.fingerCount  = fingerPacket->FingerCount;

        if (finger->TouchState == WMTFingerStateNone) {}

        else if (finger->TouchState == WMTFingerStateDown) {
            event.state = TouchState::Down;
            this->_touchDownEvent(event);
        }

        else if (finger->TouchState == WMTFingerStateHold) {
            event.state = TouchState::Hold;
            this->_touchHoldEvent(event);
        }

        else if (finger->TouchState == WMTFingerStateUp) {
            event.state = TouchState::Up;
            this->_touchUpEvent(event);
        }

        else {
            std::cerr << "Warning: Unrecognised touch state: " << finger->TouchState << std::endl;
        }
    }
}


void Touch::printAttachedDevices() const {
    int deviceIDs[MAX_ATTACHED_DEVICES] = {};
    int deviceCount = 0;

    // Ask the Wacom API for all connected touch API-capable devices.
    // Pass a comfortably large buffer so you don't have to call this method twice.
    deviceCount = WacomMTGetAttachedDeviceIDs(deviceIDs, sizeof(deviceIDs));

    if (deviceCount > MAX_ATTACHED_DEVICES) {
        std::cerr << "More tablets connected than would fit in the "
                  << "supplied buffer. Will need to reallocate buffer!";
    }
    else {       
        for(int ii = 0; ii < deviceCount; ii++) {
            int deviceID = deviceIDs[ii];

            WacomMTCapability capabilities = {};
            WacomMTGetDeviceCapabilities(deviceID, &capabilities);

            const auto type = capabilities.Type == WMTDeviceTypeIntegrated ? "Integrated"
                            : capabilities.Type == WMTDeviceTypeOpaque ? "Opaque"
                            : "Unknown";

            std::cout << "Version: "        << capabilities.Version         << std::endl
                      << "DeviceID: "       << capabilities.DeviceID        << std::endl
                      << "Type: "           << type                         << std::endl
                      << "LogicalOriginX: " << capabilities.LogicalOriginX  << std::endl
                      << "LogicalOriginY: " << capabilities.LogicalOriginY  << std::endl
                      << "LogicalWidth: "   << capabilities.LogicalWidth    << std::endl
                      << "LogicalHeight: "  << capabilities.LogicalHeight   << std::endl
                      << "PhysicalSizeX: "  << capabilities.PhysicalSizeX   << std::endl
                      << "PhysicalSizeY: "  << capabilities.PhysicalSizeY   << std::endl
                      << "ReportedSizeX: "  << capabilities.ReportedSizeX   << std::endl
                      << "ReportedSizeY: "  << capabilities.ReportedSizeY   << std::endl;
        }
    }
}


void Touch::_touchDownEvent(TouchEvent event) {
    _events[event.fingerId] = event;
    this->touchDownEvent(event);
}


void Touch::_touchHoldEvent(TouchEvent event) {
    auto& finger = _events.at(event.fingerId);

    if (finger.state == TouchState::Hold) {
        finger.x = event.x;
        finger.y = event.y;
        finger.width = event.width;
        finger.height = event.height;
    }

    this->touchHoldEvent(event);
}


void Touch::_touchUpEvent(TouchEvent event) {
    // Can happen on low-confidence events
    if (!_events.count(event.fingerId)) return;

    auto& finger = _events.at(event.fingerId);
    finger.state = TouchState::Up;
    this->touchUpEvent(event);
}


auto Touch::poll() -> const PollEvents {
    PollEvents events = _events;

    for (auto [fingerId, event] : events) {
        if (event.state == TouchState::Up) {
            _events.erase(fingerId);
        }

        // Stick keys
        // Don't actually permit Hold events until Down has been polled
        if (event.state == TouchState::Down) {
            _events.at(fingerId).state = TouchState::Hold;
        }
    }

    return events;
}


}