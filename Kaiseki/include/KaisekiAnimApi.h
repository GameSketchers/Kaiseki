#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#include "KaisekiCommon.h"

// ============================================================================
// WNDPROC INTEGRATION CONTRACT:
// The window procedure of any main frame created by KaisekiUI delegates
// WM_NCCALCSIZE to KaisekiAnim_HandleNcCalcSize and WM_NCHITTEST to
// KaisekiAnim_HitTestFrame. This ensures hardware-accelerated DWM swoosh/zoom
// transitions and Windows 11 snap layouts run natively without flickering.
// ============================================================================

#ifdef KAISEKIANIM_EXPORTS
#define KAISEKI_ANIM_API __declspec(dllexport)
#else
#define KAISEKI_ANIM_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

// System Transition Commands (Windows DWM Native Operations)
typedef enum {
    KAISEKI_ACTION_MINIMIZE = 0,
    KAISEKI_ACTION_MAXIMIZE = 1,
    KAISEKI_ACTION_RESTORE  = 2,
    KAISEKI_ACTION_CLOSE    = 3
} KaisekiWindowAction;

// Easing Curves for UI Animations & Dialog Expansions
typedef enum {
    KAISEKI_EASE_LINEAR = 0,
    KAISEKI_EASE_OUT_QUAD = 1,
    KAISEKI_EASE_OUT_CUBIC = 2,
    KAISEKI_EASE_IN_OUT_CUBIC = 3,
    KAISEKI_EASE_OUT_BACK = 4,
    KAISEKI_EASE_OUT_BOUNCE = 5,
    KAISEKI_EASE_SPRING = 6
} KaisekiEaseCurve;

// Version
KAISEKI_ANIM_API const char* KaisekiAnim_GetVersion(void);

// 1. DWM Frame Initialization (Chrome / DaVinci Resolve / Photoshop Architecture)
// Configures DWM composition frame, extends glass into client area, enables DWM transitions & rounded corners
KAISEKI_ANIM_API BOOL KaisekiAnim_InitWindowFrame(HWND hwnd, BOOL enableDwmTransitions, BOOL enableRoundedCorners);

// 2. WM_NCCALCSIZE Handler
// Completely removes default white titlebar while keeping the window classified as a top-level frame window,
// ensuring Windows default minimize/maximize/restore swoosh and zoom animations run 100% natively.
KAISEKI_ANIM_API LRESULT KaisekiAnim_HandleNcCalcSize(HWND hwnd, WPARAM wParam, LPARAM lParam, BOOL isMaximized);

// 3. WM_SYSCOMMAND Native System Transition Trigger (Plays Windows default swoosh & zoom animations)
KAISEKI_ANIM_API BOOL KaisekiAnim_PerformTransition(HWND hwnd, KaisekiWindowAction action);

// 4. Advanced Hit Test Engine (supports Windows 11 Snap Layouts flyout on Maximize button hover!)
KAISEKI_ANIM_API LRESULT KaisekiAnim_HitTestFrame(
    HWND hwnd,
    POINT ptScreen,
    RECT rcCaption,
    RECT rcMin,
    RECT rcMax,
    RECT rcClose,
    int borderThickness,
    BOOL isMaximized
);

// 5. Motion & Physics Easing Engine
KAISEKI_ANIM_API float KaisekiAnim_CalculateEase(KaisekiEaseCurve curve, float t);

// 6. Spring Physics Simulator for UI controls
KAISEKI_ANIM_API void KaisekiAnim_SpringStep(float* current, float* velocity, float target, float tension, float damping, float dt);

#ifdef __cplusplus
}
#endif
