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

#include "App.xaml.h"
#include "OpenGLESPage.xaml.h"
#include "windows.gaming.input.h"

#include "base/CCEventController.h"
#include "base/CCController.h"

using namespace CocosAppWinRT;
using namespace cocos2d;
using namespace Platform;
using namespace Concurrency;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Display;
using namespace Windows::System::Threading;
using namespace Windows::UI::Core;
using namespace Windows::UI::Input;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;

using namespace Windows::Gaming::Input;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Platform::Collections;

#if (WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP) || _MSC_VER >= 1900
using namespace Windows::Phone::UI::Input;
#endif

OpenGLESPage::OpenGLESPage() :
    OpenGLESPage(nullptr)
{
}

void OpenGLESPage::App_BackRequested(
    Platform::Object^ sender,
    Windows::UI::Core::BackRequestedEventArgs^ e)
{
    // Navigate back if possible, and if the event has not
    // already been handled.
    if (e->Handled == false)
    {
        e->Handled = true;
    }
}

OpenGLESPage::OpenGLESPage(OpenGLES* openGLES) :
    mOpenGLES(openGLES),
    mRenderSurface(EGL_NO_SURFACE),
    mCoreInput(nullptr),
    mDpi(0.0f),
    mDeviceLost(false),
    mCursorVisible(true),
    mVisible(false),
    mOrientation(DisplayOrientations::Landscape)
{
    InitializeComponent();

    Windows::UI::Core::CoreWindow^ window = Windows::UI::Xaml::Window::Current->CoreWindow;

    window->VisibilityChanged +=
        ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreWindow^, Windows::UI::Core::VisibilityChangedEventArgs^>(this, &OpenGLESPage::OnVisibilityChanged);

	window->KeyDown += ref new TypedEventHandler<CoreWindow^, KeyEventArgs^>(this, &OpenGLESPage::OnKeyPressed);

	window->KeyUp += ref new TypedEventHandler<CoreWindow^, KeyEventArgs^>(this, &OpenGLESPage::OnKeyReleased);

	window->CharacterReceived += ref new TypedEventHandler<CoreWindow^, CharacterReceivedEventArgs^>(this, &OpenGLESPage::OnCharacterReceived);


    DisplayInformation^ currentDisplayInformation = DisplayInformation::GetForCurrentView();

    currentDisplayInformation->OrientationChanged +=
        ref new TypedEventHandler<DisplayInformation^, Object^>(this, &OpenGLESPage::OnOrientationChanged);

    mOrientation = currentDisplayInformation->CurrentOrientation;

    this->Loaded +=
        ref new Windows::UI::Xaml::RoutedEventHandler(this, &OpenGLESPage::OnPageLoaded);

#if _MSC_VER >= 1900
    if (Windows::Foundation::Metadata::ApiInformation::IsTypePresent("Windows.UI.ViewManagement.StatusBar"))
    {
        Windows::UI::ViewManagement::StatusBar::GetForCurrentView()->HideAsync();
    }

    if (Windows::Foundation::Metadata::ApiInformation::IsTypePresent("Windows.Phone.UI.Input.HardwareButtons"))
    {
        HardwareButtons::BackPressed += ref new EventHandler<BackPressedEventArgs^>(this, &OpenGLESPage::OnBackButtonPressed);
	}

#else
#if (WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP)
    Windows::UI::ViewManagement::StatusBar::GetForCurrentView()->HideAsync();
    HardwareButtons::BackPressed += ref new EventHandler<BackPressedEventArgs^>(this, &OpenGLESPage::OnBackButtonPressed);
#else
    // Disable all pointer visual feedback for better performance when touching.
    // This is not supported on Windows Phone applications.
    auto pointerVisualizationSettings = Windows::UI::Input::PointerVisualizationSettings::GetForCurrentView();
    pointerVisualizationSettings->IsContactFeedbackEnabled = false;
    pointerVisualizationSettings->IsBarrelButtonFeedbackEnabled = false;
#endif
#endif

    Background = ref new SolidColorBrush(Windows::UI::Colors::Black);


    CreateInput();
}

void OpenGLESPage::CreateInput()
{
    // Register our SwapChainPanel to get independent input pointer events
    auto workItemHandler = ref new WorkItemHandler([this](IAsyncAction ^)
    {

            auto _this = this;

            m_gamepadAddedEventToken =
                Windows::Gaming::Input::Gamepad::GamepadAdded += 
                ref new EventHandler<Windows::Gaming::Input::Gamepad^ >([_this](Platform::Object^ object, Windows::Gaming::Input::Gamepad^ args)
                    {
                        _this->OnGamepadAdded(object, args);
                    });

            m_gamepadRemovedEventToken = Windows::Gaming::Input::Gamepad::GamepadRemoved +=
                ref new EventHandler<Windows::Gaming::Input::Gamepad^ >([_this](Platform::Object^ object, Windows::Gaming::Input::Gamepad^ args) {
                        _this->OnGamepadRemoved(object, args);                
                    });
            
        // The CoreIndependentInputSource will raise pointer events for the specified device types on whichever thread it's created on.
        mCoreInput = swapChainPanel->CreateCoreIndependentInputSource(
            Windows::UI::Core::CoreInputDeviceTypes::Mouse |
            Windows::UI::Core::CoreInputDeviceTypes::Touch |
            Windows::UI::Core::CoreInputDeviceTypes::Pen 
            );

        // Register for pointer events, which will be raised on the background thread.
        mCoreInput->PointerPressed += ref new TypedEventHandler<Object^, PointerEventArgs^>(this, &OpenGLESPage::OnPointerPressed);
        mCoreInput->PointerMoved += ref new TypedEventHandler<Object^, PointerEventArgs^>(this, &OpenGLESPage::OnPointerMoved);
        mCoreInput->PointerReleased += ref new TypedEventHandler<Object^, PointerEventArgs^>(this, &OpenGLESPage::OnPointerReleased);
        mCoreInput->PointerWheelChanged += ref new TypedEventHandler<Object^, PointerEventArgs^>(this, &OpenGLESPage::OnPointerWheelChanged);

        if (GLViewImpl::sharedOpenGLView() && !GLViewImpl::sharedOpenGLView()->isCursorVisible())
        {
            mCoreInput->PointerCursor = nullptr;
        }

		// Begin processing input messages as they're delivered.
        mCoreInput->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessUntilQuit);
    });

    // Run task on a dedicated high priority background thread.
    mInputLoopWorker = ThreadPool::RunAsync(workItemHandler, WorkItemPriority::High, WorkItemOptions::TimeSliced);
}

void OpenGLESPage::OnGamepadAdded(Object^, Windows::Gaming::Input::Gamepad^ args)
{
    for (auto const& gamepadWithButtonState : m_gamepads)
    {
        if (args == gamepadWithButtonState.gamepad)
        {
            // This gamepad is already in the list.
            return;
        }
    }

    auto gamepad = GamepadWithButtonState();

    gamepad.gamepad = args;
    gamepad.buttonAWasPressedLastFrame = false;

    m_gamepads.push_back(gamepad);

    int indexOf = -1;

    for (int i = 0; i < m_gamepads.size(); ++i) {
        if (m_gamepads[i].gamepad == args) {
            indexOf = i;
        }
    }

    gamepad.id = indexOf;

    cocos2d::ControllerImpl::onConnected(indexOf);
}

void OpenGLESPage::OnGamepadRemoved(Object^, Windows::Gaming::Input::Gamepad^ args)
{
    int indexOf = -1;

    for (int i = 0; i < m_gamepads.size(); ++i) {
        if (m_gamepads[i].gamepad == args) {
            indexOf = i;
        }
    }

    m_gamepads.erase(
        std::remove_if(m_gamepads.begin(), m_gamepads.end(), [&](GamepadWithButtonState& gamepadWithState)
            {
                return gamepadWithState.gamepad == args;
            }),
        m_gamepads.end());

    cocos2d::ControllerImpl::onDisconnected(indexOf);
}

std::pair<bool, Windows::Gaming::Input::GamepadButtons> 
OpenGLESPage::gamepadReading(GamepadWithButtonState gamepad, 
                                Windows::Gaming::Input::GamepadButtons button) {
    auto currentReading = gamepad.gamepad->GetCurrentReading();
    return std::pair<bool, Windows::Gaming::Input::GamepadButtons>
        (((currentReading.Buttons & button) == button), button);
}

void OpenGLESPage::submitReading(GamepadWithButtonState gamepad,
    std::pair<bool, Windows::Gaming::Input::GamepadButtons> reading) {
    if (reading.first)
    {

        cocos2d::ControllerImpl::onButtonEvent(gamepad.id,
            (int)reading.second,
            true,
            1,
            false);

    }
    else
    {

        cocos2d::ControllerImpl::onButtonEvent(gamepad.id,
            (int)reading.second,
            false,
            1,
            false);


    }
}

double OpenGLESPage::axisReading(GamepadWithButtonState gamepad, Windows::Gaming::Input::GamepadButtons button, int axis) {

    auto currentReading = gamepad.gamepad->GetCurrentReading();

    switch (button) {
    case Windows::Gaming::Input::GamepadButtons::LeftThumbstick:
        if (axis == -1) {
            return currentReading.LeftThumbstickX;
        }
        else {
            return currentReading.LeftThumbstickY;
        }
        
        break;

    case Windows::Gaming::Input::GamepadButtons::RightThumbstick:
        if (axis == -1) {
            return currentReading.RightThumbstickX;
        }
        else {
            return currentReading.RightThumbstickY;
        }
        break;

    default:
        return 0.0;
    }
}

void OpenGLESPage::setAxisReading(GamepadWithButtonState gamepad, std::pair<cocos2d::Controller::Key, double> reading) {

    auto iter = cocos2d::ControllerImpl::findController(gamepad.id);
    if (iter == Controller::s_allController.end())
    {
        CCLOG("onAxisEvent:connect new controller.");
        cocos2d::ControllerImpl::onConnected(gamepad.id);
        iter = cocos2d::ControllerImpl::findController(gamepad.id);
    }

    (*iter)->_allKeyPrevStatus[(int)reading.first] = (*iter)->_allKeyStatus[(int)reading.first];
    (*iter)->_allKeyStatus[(int)reading.first].value = reading.second;
    (*iter)->_allKeyStatus[(int)reading.first].isAnalog = true;

}

void OpenGLESPage::submitAxisReading(GamepadWithButtonState gamepad, Windows::Gaming::Input::GamepadButtons axis) {
    auto iter = cocos2d::ControllerImpl::findController(gamepad.id);
    if (iter == Controller::s_allController.end())
    {
        CCLOG("onAxisEvent:connect new controller.");
        cocos2d::ControllerImpl::onConnected(gamepad.id);
        iter = cocos2d::ControllerImpl::findController(gamepad.id);
    }

    (*iter)->onAxisEvent((int)axis, 0.0, true);
}



OpenGLESPage::~OpenGLESPage()
{
    if (m_gamepadAddedEventToken.Value != 0)
    {
        Windows::Gaming::Input::Gamepad::GamepadAdded -= m_gamepadAddedEventToken;
    }
    if (m_gamepadRemovedEventToken.Value != 0)
    {
        Windows::Gaming::Input::Gamepad::GamepadRemoved -= m_gamepadRemovedEventToken;
    }

    StopRenderLoop();
    DestroyRenderSurface();
}

void OpenGLESPage::OnPageLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    Windows::UI::Core::SystemNavigationManager::GetForCurrentView()->
        BackRequested += ref new Windows::Foundation::EventHandler<
        Windows::UI::Core::BackRequestedEventArgs^>(
            this, &OpenGLESPage::App_BackRequested);

    // The SwapChainPanel has been created and arranged in the page layout, so EGL can be initialized.
    CreateRenderSurface();
    StartRenderLoop();
    mVisible = true;
}

void OpenGLESPage::CreateRenderSurface()
{
    if (mOpenGLES && mRenderSurface == EGL_NO_SURFACE)
    {
        // The app can configure the SwapChainPanel which may boost performance.
        // By default, this template uses the default configuration.
        mRenderSurface = mOpenGLES->CreateSurface(swapChainPanel, nullptr, nullptr);

        // You can configure the SwapChainPanel to render at a lower resolution and be scaled up to
        // the swapchain panel size. This scaling is often free on mobile hardware.
        //
        // One way to configure the SwapChainPanel is to specify precisely which resolution it should render at.
        // Size customRenderSurfaceSize = Size(800, 600);
        // mRenderSurface = mOpenGLES->CreateSurface(swapChainPanel, &customRenderSurfaceSize, nullptr);
        //
        // Another way is to tell the SwapChainPanel to render at a certain scale factor compared to its size.
        // e.g. if the SwapChainPanel is 1920x1280 then setting a factor of 0.5f will make the app render at 960x640
        // float customResolutionScale = 0.5f;
        // mRenderSurface = mOpenGLES->CreateSurface(swapChainPanel, nullptr, &customResolutionScale);
        // 
    }
}

void OpenGLESPage::DestroyRenderSurface()
{
    if (mOpenGLES)
    {
        mOpenGLES->DestroySurface(mRenderSurface);
    }
    mRenderSurface = EGL_NO_SURFACE;
}

void OpenGLESPage::RecoverFromLostDevice()
{
    critical_section::scoped_lock lock(mRenderSurfaceCriticalSection);
    DestroyRenderSurface();
    mOpenGLES->Reset();
    CreateRenderSurface();
    std::unique_lock<std::mutex> locker(mSleepMutex);
    mDeviceLost = false;
    mSleepCondition.notify_one();
}

void OpenGLESPage::TerminateApp()
{
    {
        critical_section::scoped_lock lock(mRenderSurfaceCriticalSection);

        if (mOpenGLES)
        {
            mOpenGLES->DestroySurface(mRenderSurface);
            mOpenGLES->Cleanup();
        }
    }
    Windows::UI::Xaml::Application::Current->Exit();
}

void OpenGLESPage::StartRenderLoop()
{
    // If the render loop is already running then do not start another thread.
    if (mRenderLoopWorker != nullptr && mRenderLoopWorker->Status == Windows::Foundation::AsyncStatus::Started)
    {
        return;
    }

    DisplayInformation^ currentDisplayInformation = DisplayInformation::GetForCurrentView();
    mDpi = currentDisplayInformation->LogicalDpi;

    auto dispatcher = Windows::UI::Xaml::Window::Current->CoreWindow->Dispatcher;

    // Create a task for rendering that will be run on a background thread.
    auto workItemHandler = ref new Windows::System::Threading::WorkItemHandler([this, dispatcher](Windows::Foundation::IAsyncAction^ action)
        {
            mOpenGLES->MakeCurrent(mRenderSurface);

            GLsizei panelWidth = 0;
            GLsizei panelHeight = 0;
            mOpenGLES->GetSurfaceDimensions(mRenderSurface, &panelWidth, &panelHeight);

            if (mRenderer.get() == nullptr)
            {
                mRenderer = std::make_shared<Cocos2dRenderer>(panelWidth, panelHeight, mDpi, mOrientation, dispatcher, swapChainPanel);
            }

            mRenderer->Resume();

            while (action->Status == Windows::Foundation::AsyncStatus::Started)
            {
                if (!mVisible)
                {
                    mRenderer->Pause();
                }

                // wait until app is visible again or thread is cancelled
                while (!mVisible)
                {
                    std::unique_lock<std::mutex> lock(mSleepMutex);
                    mSleepCondition.wait(lock);

                    if (action->Status != Windows::Foundation::AsyncStatus::Started)
                    {
                        return; // thread was cancelled. Exit thread
                    }

                    if (mVisible)
                    {
                        mRenderer->Resume();
                    }
                    else // spurious wake up
                    {
                        continue;
                    }
                }

                mOpenGLES->GetSurfaceDimensions(mRenderSurface, &panelWidth, &panelHeight);
                mRenderer.get()->Draw(panelWidth, panelHeight, mDpi, mOrientation);

                // Recreate input dispatch
                if (GLViewImpl::sharedOpenGLView() && mCursorVisible != GLViewImpl::sharedOpenGLView()->isCursorVisible())
                {
                    CreateInput();
                    mCursorVisible = GLViewImpl::sharedOpenGLView()->isCursorVisible();
                }

                // Read gamepad input

                // Check for new input state since the last frame.
                for (int i = 0; i < m_gamepads.size(); ++i)
                {
                    auto& gamepadWithButtonState = m_gamepads[i];

                    auto buttonDownThisUpdate = gamepadReading(gamepadWithButtonState, Windows::Gaming::Input::GamepadButtons::Menu);
                    submitReading(gamepadWithButtonState, buttonDownThisUpdate);

                    buttonDownThisUpdate = gamepadReading(gamepadWithButtonState, Windows::Gaming::Input::GamepadButtons::View);
                    submitReading(gamepadWithButtonState, buttonDownThisUpdate);

                    buttonDownThisUpdate = gamepadReading(gamepadWithButtonState, Windows::Gaming::Input::GamepadButtons::A);
                    submitReading(gamepadWithButtonState, buttonDownThisUpdate);

                    buttonDownThisUpdate = gamepadReading(gamepadWithButtonState, Windows::Gaming::Input::GamepadButtons::B);
                    submitReading(gamepadWithButtonState, buttonDownThisUpdate);

                    buttonDownThisUpdate = gamepadReading(gamepadWithButtonState, Windows::Gaming::Input::GamepadButtons::X);
                    submitReading(gamepadWithButtonState, buttonDownThisUpdate);

                    buttonDownThisUpdate = gamepadReading(gamepadWithButtonState, Windows::Gaming::Input::GamepadButtons::Y);
                    submitReading(gamepadWithButtonState, buttonDownThisUpdate);


                    buttonDownThisUpdate = gamepadReading(gamepadWithButtonState, Windows::Gaming::Input::GamepadButtons::DPadUp);
                    submitReading(gamepadWithButtonState, buttonDownThisUpdate);

                    buttonDownThisUpdate = gamepadReading(gamepadWithButtonState, Windows::Gaming::Input::GamepadButtons::DPadDown);
                    submitReading(gamepadWithButtonState, buttonDownThisUpdate);

                    buttonDownThisUpdate = gamepadReading(gamepadWithButtonState, Windows::Gaming::Input::GamepadButtons::DPadLeft);
                    submitReading(gamepadWithButtonState, buttonDownThisUpdate);

                    buttonDownThisUpdate = gamepadReading(gamepadWithButtonState, Windows::Gaming::Input::GamepadButtons::DPadRight);
                    submitReading(gamepadWithButtonState, buttonDownThisUpdate);


                    buttonDownThisUpdate = gamepadReading(gamepadWithButtonState, Windows::Gaming::Input::GamepadButtons::LeftShoulder);
                    submitReading(gamepadWithButtonState, buttonDownThisUpdate);

                    buttonDownThisUpdate = gamepadReading(gamepadWithButtonState, Windows::Gaming::Input::GamepadButtons::RightShoulder);
                    submitReading(gamepadWithButtonState, buttonDownThisUpdate);

                    double leftThumbstickXAxisReading = axisReading(gamepadWithButtonState, Windows::Gaming::Input::GamepadButtons::LeftThumbstick, -1);
                    double leftThumbstickYAxisReading = axisReading(gamepadWithButtonState, Windows::Gaming::Input::GamepadButtons::LeftThumbstick, 1);

                    setAxisReading(gamepadWithButtonState, { cocos2d::Controller::Key::JOYSTICK_LEFT_X, leftThumbstickXAxisReading });
                    setAxisReading(gamepadWithButtonState, { cocos2d::Controller::Key::JOYSTICK_LEFT_Y, leftThumbstickYAxisReading });

                    submitAxisReading(gamepadWithButtonState, Windows::Gaming::Input::GamepadButtons::LeftThumbstick);

                    double rightThumbstickXAxisReading = axisReading(gamepadWithButtonState, Windows::Gaming::Input::GamepadButtons::RightThumbstick, -1);
                    double rightThumbstickYAxisReading = axisReading(gamepadWithButtonState, Windows::Gaming::Input::GamepadButtons::RightThumbstick, 1);

                    setAxisReading(gamepadWithButtonState, { cocos2d::Controller::Key::JOYSTICK_RIGHT_X, rightThumbstickXAxisReading });
                    setAxisReading(gamepadWithButtonState, { cocos2d::Controller::Key::JOYSTICK_RIGHT_Y, rightThumbstickYAxisReading });

                    submitAxisReading(gamepadWithButtonState, Windows::Gaming::Input::GamepadButtons::RightThumbstick);


            }

            if (mRenderer->AppShouldExit())
            {
                // run on main UI thread
                swapChainPanel->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::High, ref new DispatchedHandler([=]()
                {
                    TerminateApp();
                }));

                return;
            }

            EGLBoolean result = GL_FALSE;
            {
                critical_section::scoped_lock lock(mRenderSurfaceCriticalSection);
                result = mOpenGLES->SwapBuffers(mRenderSurface);
            }

            if (result != GL_TRUE)
            {
                // The call to eglSwapBuffers was not be successful (i.e. due to Device Lost)
                // If the call fails, then we must reinitialize EGL and the GL resources.
                mRenderer->Pause();
                mDeviceLost = true;

                // XAML objects like the SwapChainPanel must only be manipulated on the UI thread.
                swapChainPanel->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::High, ref new Windows::UI::Core::DispatchedHandler([=]()
                {
                    RecoverFromLostDevice();
                }, CallbackContext::Any));

                // wait until OpenGL is reset or thread is cancelled
                while (mDeviceLost)
                {
                    std::unique_lock<std::mutex> lock(mSleepMutex);
                    mSleepCondition.wait(lock);

                    if (action->Status != Windows::Foundation::AsyncStatus::Started)
                    {
                        return; // thread was cancelled. Exit thread
                    }

                    if (!mDeviceLost)
                    {
                        mOpenGLES->MakeCurrent(mRenderSurface);
                        // restart cocos2d-x 
                        mRenderer->DeviceLost();
                    }
                    else // spurious wake up
                    {
                        continue;
                    }
                }
            }
        }
    });

    // Run task on a dedicated high priority background thread.
    mRenderLoopWorker = Windows::System::Threading::ThreadPool::RunAsync(workItemHandler, Windows::System::Threading::WorkItemPriority::High, Windows::System::Threading::WorkItemOptions::TimeSliced);
}

void OpenGLESPage::StopRenderLoop()
{
    if (mRenderLoopWorker)
    {
        mRenderLoopWorker->Cancel();
        std::unique_lock<std::mutex> locker(mSleepMutex);
        mSleepCondition.notify_one();
        mRenderLoopWorker = nullptr;
    }
}

void OpenGLESPage::OnPointerPressed(Object^ sender, PointerEventArgs^ e)
{
    bool isMouseEvent = e->CurrentPoint->PointerDevice->PointerDeviceType == Windows::Devices::Input::PointerDeviceType::Mouse;
    if (mRenderer)
    {
        mRenderer->QueuePointerEvent(isMouseEvent ? PointerEventType::MousePressed : PointerEventType::PointerPressed, e);
    }
}

void OpenGLESPage::OnPointerMoved(Object^ sender, PointerEventArgs^ e)
{
    bool isMouseEvent = e->CurrentPoint->PointerDevice->PointerDeviceType == Windows::Devices::Input::PointerDeviceType::Mouse;
    if (mRenderer)
    {
        mRenderer->QueuePointerEvent(isMouseEvent ? PointerEventType::MouseMoved : PointerEventType::PointerMoved, e);
    }
}

void OpenGLESPage::OnPointerReleased(Object^ sender, PointerEventArgs^ e)
{
    bool isMouseEvent = e->CurrentPoint->PointerDevice->PointerDeviceType == Windows::Devices::Input::PointerDeviceType::Mouse;

    if (mRenderer)
    {
        mRenderer->QueuePointerEvent(isMouseEvent ? PointerEventType::MouseReleased : PointerEventType::PointerReleased, e);
    }
}

void OpenGLESPage::OnPointerWheelChanged(Object^ sender, PointerEventArgs^ e)
{
    bool isMouseEvent = e->CurrentPoint->PointerDevice->PointerDeviceType == Windows::Devices::Input::PointerDeviceType::Mouse;
    if (mRenderer && isMouseEvent)
    {
        mRenderer->QueuePointerEvent(PointerEventType::MouseWheelChanged, e);
    }
}

void OpenGLESPage::OnKeyPressed(CoreWindow^ sender, KeyEventArgs^ e)
{
    log("OpenGLESPage::OnKeyPressed %d", e->VirtualKey);
    if (mRenderer)
    {
        mRenderer->QueueKeyboardEvent(WinRTKeyboardEventType::KeyPressed, e);
    }
}

void OpenGLESPage::OnCharacterReceived(CoreWindow^ sender, CharacterReceivedEventArgs^ e)
{
#if 0
    if (!e->KeyStatus.WasKeyDown)
    {
        log("OpenGLESPage::OnCharacterReceived %d", e->KeyCode);
    }
#endif
}

void OpenGLESPage::OnKeyReleased(CoreWindow^ sender, KeyEventArgs^ e)
{
    log("OpenGLESPage::OnKeyReleased %d", e->VirtualKey);
    if (mRenderer)
    {
        mRenderer->QueueKeyboardEvent(WinRTKeyboardEventType::KeyReleased, e);
    }
}


void OpenGLESPage::OnOrientationChanged(DisplayInformation^ sender, Object^ args)
{
    mOrientation = sender->CurrentOrientation;
}

void OpenGLESPage::SetVisibility(bool isVisible)
{
    if (isVisible && mRenderSurface != EGL_NO_SURFACE)
    {
        if (!mVisible)
        {
            std::unique_lock<std::mutex> locker(mSleepMutex);
            mVisible = true;
            mSleepCondition.notify_one();
        }
    }
    else
    {
        mVisible = false;
    }
}

void OpenGLESPage::OnVisibilityChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::VisibilityChangedEventArgs^ args)
{
    if (args->Visible && mRenderSurface != EGL_NO_SURFACE)
    {
        SetVisibility(true);
    }
    else
    {
        SetVisibility(false);
    }
}

#if (WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP) || _MSC_VER >= 1900
/*
We set args->Handled = true to prevent the app from quitting when the back button is pressed.
This is because this back button event happens on the XAML UI thread and not the cocos2d-x UI thread.
We need to give the game developer a chance to decide to exit the app depending on where they
are in their game. They can receive the back button event by listening for the
EventKeyboard::KeyCode::KEY_ESCAPE event.

The default behavior is to exit the app if the  EventKeyboard::KeyCode::KEY_ESCAPE event
is not handled by the game.
*/
void OpenGLESPage::OnBackButtonPressed(Object^ sender, BackPressedEventArgs^ args)
{
    if (mRenderer)
    {
        mRenderer->QueueBackButtonEvent();
        args->Handled = true;
    }
}
#endif