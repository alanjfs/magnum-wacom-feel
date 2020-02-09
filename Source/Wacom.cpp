#include <iostream>
#include "Wacom.h"


namespace Wacom {


static void OnAttached(WacomMTCapability deviceInfo, void *userInfo) {
    Tablet* wacomFeelObj = (Tablet*)userInfo;
    wacomFeelObj->_deviceAttached(deviceInfo);
}


static void OnDetached(int deviceID, void *userInfo) {
    Tablet* wacomFeelObj = (Tablet*)userInfo;
    wacomFeelObj->_deviceDetached(deviceID);
}


static int OnFinger(WacomMTFingerCollection *fingerPacket, void *userInfo) {
    Tablet* wacomFeelObj = (Tablet*)userInfo;
    wacomFeelObj->_fingerCallBack(fingerPacket);
    return 0;
}


bool Tablet::init() {
    bool wasInitialised = false;

    WacomMTError err = WacomMTInitialize(WACOM_MULTI_TOUCH_API_VERSION);

    if (err == WMTErrorSuccess) {
        std::cout << "Successfully initialised the Wacom SDK.\n";

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


void Tablet::_deviceAttached(WacomMTCapability deviceInfo) {
    std::cout << "Tablet::_deviceAttached";
}


void Tablet::_deviceDetached(int deviceID) {
    std::cout << "Tablet::_deviceDetached";    
}


void Tablet::_fingerCallBack(WacomMTFingerCollection *fingerPacket) {
    // std::cout << "fingerPacket->FingerCount: " << fingerPacket->FingerCount << std::endl;

    for(int fingerIndex = 0; fingerIndex < fingerPacket->FingerCount; fingerIndex++)
    {
        WacomMTFinger* finger = &fingerPacket->Fingers[fingerIndex];

        // From http://www.wacomeng.com/touch/WacomFeelMulti-TouchAPI.htm
        // Confidence: If true the driver believes this is a valid touch from a finger.  
        // If false the driver thinks this may be an accidental touch, forearm or palm.

        TouchEvent event{};
        event.id           = finger->FingerID;
        event.x            = finger->X;
        event.y            = finger->Y;            
        event.numTouches   = fingerPacket->FingerCount;
        event.width        = finger->Width;
        event.height       = finger->Height;
        event.angle        = finger->Orientation;
        event.confidence   = finger->Confidence;

        if (finger->TouchState == WMTFingerStateNone) {}

        else if (finger->TouchState == WMTFingerStateDown) {
            TouchDownEvent(event);
        }

        else if (finger->TouchState == WMTFingerStateHold) {
            TouchHoldEvent(event);
        }

        else if (finger->TouchState == WMTFingerStateUp) {
            TouchUpEvent(event);
        }
    }
}


void Tablet::listAttachedDevices() {
    int   deviceIDs[MAX_ATTACHED_DEVICES]  = {};
    int   deviceCount    = 0;

    // Ask the Wacom API for all connected touch API-capable devices.
    // Pass a comfortably large buffer so you don't have to call this method twice.
    deviceCount = WacomMTGetAttachedDeviceIDs(deviceIDs, sizeof(deviceIDs));

    if (deviceCount > MAX_ATTACHED_DEVICES) {
        // With a number as big as MAX_ATTACHED_DEVICES, this will never actually happen.
        std::cerr << "More tablets connected than would fit in the "
                  << "supplied buffer. Will need to reallocate buffer!";
    }
    else {       
        for(int ii = 0; ii < deviceCount; ii++) {
            int deviceID = deviceIDs[ii];

            WacomMTCapability capabilities = {};
            WacomMTGetDeviceCapabilities(deviceID, &capabilities);

            std::cout << "Device ID: " << deviceID << "  ";
            if (capabilities.Type == WMTDeviceTypeIntegrated) { 
                std::cout << "Type: Integrated\n";
            } 
            else if (capabilities.Type == WMTDeviceTypeOpaque) { 
                std::cout << "Type: Opaque\n";
            } 
            else { 
                std::cout << "Type: Unknown\n";
            }

            std::cout << "Max Fingers: " << capabilities.FingerMax
                      << "Scan size: " << capabilities.ReportedSizeX << ", " << capabilities.ReportedSizeY
                      << std::endl;
        }
    }
}

}