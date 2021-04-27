﻿/*
* cocos2d-x   http://www.cocos2d-x.org
*
* Copyright (c) 2010-2014 - cocos2d-x community
*
* Portions Copyright (c) Microsoft Open Technologies, Inc.
* All Rights Reserved
*
* Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an
* "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and limitations under the License.
*/

#pragma once

#include "OpenGLES.h"
#include "OpenGLESPage.g.h"
#include <memory>
#include <condition_variable>
#include <mutex>
#include "windows.gaming.input.h"

#include "Cocos2dRenderer.h"

#if (CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID || CC_TARGET_PLATFORM == CC_PLATFORM_IOS || CC_TARGET_PLATFORM == CC_PLATFORM_WINRT)
#include "base/CCEventListenerController.h"
#include "base/CCEventController.h"
#include "base/CCController.h"
#endif


using namespace ABI::Windows::Gaming::Input;

namespace CocosAppWinRT
{
    public ref class OpenGLESPage sealed
    {
    public:
        OpenGLESPage();
        virtual ~OpenGLESPage();
        void SetVisibility(bool isVisible);
        void App_BackRequested(Platform::Object^ sender, Windows::UI::Core::BackRequestedEventArgs^ e);

    internal:
        OpenGLESPage(OpenGLES* openGLES);

    private:
        void OnPageLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnVisibilityChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::VisibilityChangedEventArgs^ args);
#if (WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP) || _MSC_VER >= 1900
        void OnBackButtonPressed(Platform::Object^ sender, Windows::Phone::UI::Input::BackPressedEventArgs^ args);
#endif
        void CreateRenderSurface();
        void DestroyRenderSurface();
        void RecoverFromLostDevice();
        void TerminateApp();
        void StartRenderLoop();
        void StopRenderLoop();

        void CreateInput();

        ////////// Gamepad ///////////

        void OnGamepadAdded(Platform::Object^, Windows::Gaming::Input::Gamepad^ args);
        void OnGamepadRemoved(Platform::Object^, Windows::Gaming::Input::Gamepad^ args);
        Windows::Foundation::EventRegistrationToken                     m_gamepadAddedEventToken;
        Windows::Foundation::EventRegistrationToken                     m_gamepadRemovedEventToken;


        struct GamepadWithButtonState
        {
            Windows::Gaming::Input::Gamepad^ gamepad;
            int id;
            bool buttonAWasPressedLastFrame = false;
        };
        std::vector<GamepadWithButtonState>                             m_gamepads;


        bool m_pointerPressed = false;

        std::pair<bool, Windows::Gaming::Input::GamepadButtons>
            gamepadReading(GamepadWithButtonState gamepad,
                Windows::Gaming::Input::GamepadButtons button);

        void submitReading(GamepadWithButtonState gamepad,
            std::pair<bool, Windows::Gaming::Input::GamepadButtons> reading);

        double axisReading(GamepadWithButtonState gamepad, Windows::Gaming::Input::GamepadButtons button, int axis);

        void setAxisReading(GamepadWithButtonState gamepad, std::pair<cocos2d::Controller::Key, double> reading);
        void submitAxisReading(GamepadWithButtonState gamepad, Windows::Gaming::Input::GamepadButtons axis);

        //////////////////////////////

        OpenGLES* mOpenGLES;
        std::shared_ptr<Cocos2dRenderer> mRenderer;

        EGLSurface mRenderSurface;     // This surface is associated with a swapChainPanel on the page
        Concurrency::critical_section mRenderSurfaceCriticalSection;
        Windows::Foundation::IAsyncAction^ mRenderLoopWorker;

        // Track user input on a background worker thread.
        Windows::Foundation::IAsyncAction^ mInputLoopWorker;
        Windows::UI::Core::CoreIndependentInputSource^ mCoreInput;

        // Independent touch and pen handling functions.
        void OnPointerPressed(Platform::Object^ sender, Windows::UI::Core::PointerEventArgs^ e);
        void OnPointerMoved(Platform::Object^ sender, Windows::UI::Core::PointerEventArgs^ e);
        void OnPointerReleased(Platform::Object^ sender, Windows::UI::Core::PointerEventArgs^ e);
        void OnPointerWheelChanged(Platform::Object^ sender, Windows::UI::Core::PointerEventArgs^ e);

        // Independent keyboard handling functions.
		void OnKeyPressed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::KeyEventArgs^ args);
		void OnKeyReleased(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::KeyEventArgs^ args);

		void OnCharacterReceived(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::CharacterReceivedEventArgs^ args);

        void OnOrientationChanged(Windows::Graphics::Display::DisplayInformation^ sender, Platform::Object^ args);

        float mDpi;
        bool mDeviceLost;
        bool mVisible;
        bool mCursorVisible;
        Windows::Graphics::Display::DisplayOrientations mOrientation;

        std::mutex mSleepMutex;
        std::condition_variable mSleepCondition;
    };
}
