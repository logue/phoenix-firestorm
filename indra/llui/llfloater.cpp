/**
 * @file llfloater.cpp
 * @brief LLFloater base class
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

// Floating "windows" within the GL display, like the inventory floater,
// mini-map floater, etc.

#include "linden_common.h"
#include "llviewereventrecorder.h"
#include "llfloater.h"

#include "llfocusmgr.h"

#include "lluictrlfactory.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcriticaldamp.h" // LLSmoothInterpolation
#include "lldir.h"
#include "lldraghandle.h"
#include "llfloaterreg.h"
#include "llfocusmgr.h"
#include "llresizebar.h"
#include "llresizehandle.h"
#include "llkeyboard.h"
#include "llmenugl.h"   // MENU_BAR_HEIGHT
#include "llmodaldialog.h"
#include "lltextbox.h"
#include "llresmgr.h"
#include "llui.h"
#include "llwindow.h"
#include "llstl.h"
#include "llcontrol.h"
#include "lltabcontainer.h"
#include "v2math.h"
#include "lltrans.h"
#include "llhelp.h"
#include "llmultifloater.h"
#include "llsdutil.h"
#include "lluiusage.h"


// use this to control "jumping" behavior when Ctrl-Tabbing
const S32 TABBED_FLOATER_OFFSET = 0;

const F32 LLFloater::CONTEXT_CONE_IN_ALPHA = 0.0f;
const F32 LLFloater::CONTEXT_CONE_OUT_ALPHA = 1.f;
const F32 LLFloater::CONTEXT_CONE_FADE_TIME = 0.08f;

namespace LLInitParam
{
    void TypeValues<LLFloaterEnums::EOpenPositioning>::declareValues()
    {
        declare("relative",   LLFloaterEnums::POSITIONING_RELATIVE);
        declare("cascading",  LLFloaterEnums::POSITIONING_CASCADING);
        declare("centered",   LLFloaterEnums::POSITIONING_CENTERED);
        declare("specified",  LLFloaterEnums::POSITIONING_SPECIFIED);
    }
}

std::string LLFloater::sButtonNames[BUTTON_COUNT] =
{
    "llfloater_close_btn",      //BUTTON_CLOSE
    // <FS:Ansariel> FIRE-11724: Snooze group chat
    "llfloater_snooze_btn",     //BUTTON_SNOOZE
    // </FS:Ansariel>
    "llfloater_restore_btn",    //BUTTON_RESTORE
    "llfloater_minimize_btn",   //BUTTON_MINIMIZE
    "llfloater_tear_off_btn",   //BUTTON_TEAR_OFF
    "llfloater_dock_btn",       //BUTTON_DOCK
    "llfloater_help_btn"        //BUTTON_HELP
};

std::string LLFloater::sButtonToolTips[BUTTON_COUNT];

std::string LLFloater::sButtonToolTipsIndex[BUTTON_COUNT]=
{
#ifdef LL_DARWIN
    "BUTTON_CLOSE_DARWIN",  //"Close (Cmd-W)",  //BUTTON_CLOSE
#else
    "BUTTON_CLOSE_WIN",     //"Close (Ctrl-W)", //BUTTON_CLOSE
#endif
    // <FS:Ansariel> FIRE-11724: Snooze group chat
    "BUTTON_SNOOZE",        //"Snooze",     //BOTTON_SNOOZE
    // </FS:Ansariel>
    "BUTTON_RESTORE",       //"Restore",    //BUTTON_RESTORE
    "BUTTON_MINIMIZE",      //"Minimize",   //BUTTON_MINIMIZE
    "BUTTON_TEAR_OFF",      //"Tear Off",   //BUTTON_TEAR_OFF
    "BUTTON_DOCK",
    "BUTTON_HELP"
};

LLFloater::click_callback LLFloater::sButtonCallbacks[BUTTON_COUNT] =
{
    LLFloater::onClickClose,    //BUTTON_CLOSE
    // <FS:Ansariel> FIRE-11724: Snooze group chat
    LLFloater::onClickSnooze,   //BUTTON_SNOOZE
    // </FS:Ansariel>
    LLFloater::onClickMinimize, //BUTTON_RESTORE
    LLFloater::onClickMinimize, //BUTTON_MINIMIZE
    LLFloater::onClickTearOff,  //BUTTON_TEAR_OFF
    LLFloater::onClickDock,     //BUTTON_DOCK
    LLFloater::onClickHelp      //BUTTON_HELP
};

LLMultiFloater* LLFloater::sHostp = NULL;
bool            LLFloater::sQuitting = false; // Flag to prevent storing visibility controls while quitting

LLFloaterView* gFloaterView = NULL;

/*==========================================================================*|
// DEV-38598: The fundamental problem with this operation is that it can only
// support a subset of LLSD values. While it's plausible to compare two arrays
// lexicographically, what strict ordering can you impose on maps?
// (LLFloaterTOS's current key is an LLSD map.)

// Of course something like this is necessary if you want to build a std::set
// or std::map with LLSD keys. Fortunately we're getting by with other
// container types for now.

//static
bool LLFloater::KeyCompare::compare(const LLSD& a, const LLSD& b)
{
    if (a.type() != b.type())
    {
        //LL_ERRS() << "Mismatched LLSD types: (" << a << ") mismatches (" << b << ")" << LL_ENDL;
        return false;
    }
    else if (a.isUndefined())
        return false;
    else if (a.isInteger())
        return a.asInteger() < b.asInteger();
    else if (a.isReal())
        return a.asReal() < b.asReal();
    else if (a.isString())
        return a.asString() < b.asString();
    else if (a.isUUID())
        return a.asUUID() < b.asUUID();
    else if (a.isDate())
        return a.asDate() < b.asDate();
    else if (a.isURI())
        return a.asString() < b.asString(); // compare URIs as strings
    else if (a.isBoolean())
        return a.asBoolean() < b.asBoolean();
    else
        return false; // no valid operation for Binary
}
|*==========================================================================*/

bool LLFloater::KeyCompare::equate(const LLSD& a, const LLSD& b)
{
    return llsd_equals(a, b);
}

//************************************

LLFloater::Params::Params()
:   title("title"),
    short_title("short_title"),
    single_instance("single_instance", false),
    reuse_instance("reuse_instance", false),
    can_resize("can_resize", false),
    can_minimize("can_minimize", true),
    can_close("can_close", true),
    can_snooze("can_snooze", false),        // <FS:Ansariel> FIRE-11724: Snooze group chat
    can_drag_on_left("can_drag_on_left", false),
    drop_shadow("drop_shadow",true),        // <FS:Zi> Optional Drop Shadows
    label_v_padding("label_v_padding", -1), // <FS:Zi> Make vertical label padding a per-skin option
    can_tear_off("can_tear_off", true),
    save_dock_state("save_dock_state", false),
    save_rect("save_rect", false),
    save_visibility("save_visibility", false),
    can_dock("can_dock", false),
    show_title("show_title", true),
    auto_close("auto_close", false),
    positioning("positioning", LLFloaterEnums::POSITIONING_RELATIVE),
    header_height("header_height", 0),
    legacy_header_height("legacy_header_height", 0),
    close_image("close_image"),
    snooze_image("snooze_image"),       // <FS:Ansariel> FIRE-11724: Snooze group chat
    restore_image("restore_image"),
    minimize_image("minimize_image"),
    tear_off_image("tear_off_image"),
    dock_image("dock_image"),
    help_image("help_image"),
    close_pressed_image("close_pressed_image"),
    snooze_pressed_image("snooze_pressed_image"),       // <FS:Ansariel> FIRE-11724: Snooze group chat
    restore_pressed_image("restore_pressed_image"),
    minimize_pressed_image("minimize_pressed_image"),
    tear_off_pressed_image("tear_off_pressed_image"),
    dock_pressed_image("dock_pressed_image"),
    help_pressed_image("help_pressed_image"),
    open_callback("open_callback"),
    close_callback("close_callback"),
    follows("follows"),
    rel_x("rel_x", 0),
    rel_y("rel_y", 0),
    hosted_floater_show_titlebar("hosted_floater_show_titlebar", true) // <FS:Ansariel> MultiFloater without titlebar for hosted floater
{
    changeDefault(visible, false);
}


//static
const LLFloater::Params& LLFloater::getDefaultParams()
{
    return LLUICtrlFactory::getDefaultParams<LLFloater>();
}

//static
void LLFloater::initClass()
{
    // translate tooltips for floater buttons
    for (S32 i = 0; i < BUTTON_COUNT; i++)
    {
        sButtonToolTips[i] = LLTrans::getString( sButtonToolTipsIndex[i] );
    }

    LLControlVariable* ctrl = LLUI::getInstance()->mSettingGroups["config"]->getControl("ActiveFloaterTransparency").get();
    if (ctrl)
    {
        ctrl->getSignal()->connect(boost::bind(&LLFloater::updateActiveFloaterTransparency));
        updateActiveFloaterTransparency();
    }

    ctrl = LLUI::getInstance()->mSettingGroups["config"]->getControl("InactiveFloaterTransparency").get();
    if (ctrl)
    {
        ctrl->getSignal()->connect(boost::bind(&LLFloater::updateInactiveFloaterTransparency));
        updateInactiveFloaterTransparency();
    }

}

// defaults for floater param block pulled from widgets/floater.xml
static LLWidgetNameRegistry::StaticRegistrar sRegisterFloaterParams(&typeid(LLFloater::Params), "floater");

LLFloater::LLFloater(const LLSD& key, const LLFloater::Params& p)
:   LLPanel(),  // intentionally do not pass params here, see initFromParams
    mDragHandle(NULL),
    mLabelVPadding(p.label_v_padding),  // <FS:Zi> Make vertical label padding a per-skin optional
    mTitle(p.title),
    mShortTitle(p.short_title),
    mSingleInstance(p.single_instance),
    mReuseInstance(p.reuse_instance.isProvided() ? p.reuse_instance : p.single_instance), // reuse single-instance floaters by default
    mKey(key),
    mCanTearOff(p.can_tear_off),
    mCanMinimize(p.can_minimize),
    mCanClose(p.can_close),
    mCanSnooze(p.can_snooze),       // <FS:Ansariel> FIRE-11724: Snooze group chat
    mDragOnLeft(p.can_drag_on_left),
    mResizable(p.can_resize),
    mAutoClose(p.auto_close),
    mPositioning(p.positioning),
    mMinWidth(p.min_width),
    mMinHeight(p.min_height),
    mHeaderHeight(p.header_height),
    mLegacyHeaderHeight(p.legacy_header_height),
    mDefaultRectForGroup(true),
    mMinimized(false),
    mForeground(false),
    mFirstLook(true),
    mButtonScale(1.0f),
    mAutoFocus(true), // automatically take focus when opened
    mCanDock(false),
    mDocked(false),
    mTornOff(false),
    mHasBeenDraggedWhileMinimized(false),
    mPreviousMinimizedBottom(0),
    mPreviousMinimizedLeft(0),
    mDefaultRelativeX(p.rel_x),
    mDefaultRelativeY(p.rel_y),
    mMinimizeSignal(NULL),
    mHostedFloaterShowtitlebar(p.hosted_floater_show_titlebar) // <FS:Ansariel> MultiFloater without titlebar for hosted floater
//  mNotificationContext(NULL)
{
    mPosition.setFloater(*this);
//  mNotificationContext = new LLFloaterNotificationContext(getHandle());

    // Clicks stop here.
    setMouseOpaque(true);

    // Floaters always draw their background, unlike every other panel.
    setBackgroundVisible(true);

    // Floaters start not minimized.  When minimized, they save their
    // prior rectangle to be used on restore.
    mExpandedRect.set(0,0,0,0);

    memset(mButtonsEnabled, 0, BUTTON_COUNT * sizeof(bool));
    memset(mButtons, 0, BUTTON_COUNT * sizeof(LLButton*));

    // <FS:Zi> Make vertical label padding a per-skin option
    // if no padding is set, use default from settings.xml
    if (mLabelVPadding == -1)
    {
        mLabelVPadding = LLUI::getInstance()->mSettingGroups["config"]->getS32("UIFloaterTitleVPad");
    }
    // </FS:Zi>

    addDragHandle();
    addResizeCtrls();

    initFromParams(p);

    initFloater(p);
}

// Note: Floaters constructed from XML call init() twice!
void LLFloater::initFloater(const Params& p)
{
    // Close button.
    if (mCanClose)
    {
        mButtonsEnabled[BUTTON_CLOSE] = true;
    }

    // Help button: '?'
    //SL-14050 Disable all Help question marks
    // <FS:Ansariel> Nope!
    mButtonsEnabled[BUTTON_HELP] = !mHelpTopic.empty();// false;

    // Minimize button only for top draggers
    if ( !mDragOnLeft && mCanMinimize )
    {
        mButtonsEnabled[BUTTON_MINIMIZE] = true;
    }

    if(mCanDock)
    {
        mButtonsEnabled[BUTTON_DOCK] = true;
    }

    // <FS:Ansariel> FIRE-11724: Snooze group chat
    if (mCanSnooze)
    {
        mButtonsEnabled[BUTTON_SNOOZE] = true;
    }
    // </FS:Ansariel>

    buildButtons(p);

    // Floaters are created in the invisible state
    setVisible(false);

    if (!getParent())
    {
        gFloaterView->addChild(this);
    }
}

void LLFloater::addDragHandle()
{
    if (!mDragHandle)
    {
        if (mDragOnLeft)
        {
            LLDragHandleLeft::Params p;
            p.name("drag");
            p.follows.flags(FOLLOWS_ALL);
            p.label(mTitle);
            mDragHandle = LLUICtrlFactory::create<LLDragHandleLeft>(p);
        }
        else // drag on top
        {
            LLDragHandleTop::Params p;
            p.name("Drag Handle");
            p.follows.flags(FOLLOWS_ALL);
            p.label(mTitle);
            p.label_v_padding = mLabelVPadding;     // <FS:Zi> Make vertical label padding a per-skin option
            mDragHandle = LLUICtrlFactory::create<LLDragHandleTop>(p);
        }
        addChild(mDragHandle);
    }
    layoutDragHandle();
    applyTitle();
}

void LLFloater::layoutDragHandle()
{
    static LLUICachedControl<S32> floater_close_box_size ("UIFloaterCloseBoxSize", 0);
    S32 close_box_size = mCanClose ? floater_close_box_size : 0;

    LLRect rect;
    if (mDragOnLeft)
    {
        rect.setLeftTopAndSize(0, 0, DRAG_HANDLE_WIDTH, getRect().getHeight() - LLPANEL_BORDER_WIDTH - close_box_size);
    }
    else // drag on top
    {
        rect = getLocalRect();
    }
    mDragHandle->setShape(rect);
    updateTitleButtons();
}

// static
void LLFloater::updateActiveFloaterTransparency()
{
    static LLCachedControl<F32> active_transparency(*LLUI::getInstance()->mSettingGroups["config"], "ActiveFloaterTransparency", 1.f);
    sActiveControlTransparency = active_transparency;
}

// static
void LLFloater::updateInactiveFloaterTransparency()
{
    static LLCachedControl<F32> inactive_transparency(*LLUI::getInstance()->mSettingGroups["config"], "InactiveFloaterTransparency", 0.95f);
    sInactiveControlTransparency = inactive_transparency;
}

void LLFloater::addResizeCtrls()
{
    // Resize bars (sides)
    LLResizeBar::Params p;
    p.name("resizebar_left");
    p.resizing_view(this);
    p.min_size(mMinWidth);
    p.side(LLResizeBar::LEFT);
    mResizeBar[LLResizeBar::LEFT] = LLUICtrlFactory::create<LLResizeBar>(p);
    addChild( mResizeBar[LLResizeBar::LEFT] );

    p.name("resizebar_top");
    p.min_size(mMinHeight);
    p.side(LLResizeBar::TOP);

    mResizeBar[LLResizeBar::TOP] = LLUICtrlFactory::create<LLResizeBar>(p);
    addChild( mResizeBar[LLResizeBar::TOP] );

    p.name("resizebar_right");
    p.min_size(mMinWidth);
    p.side(LLResizeBar::RIGHT);
    mResizeBar[LLResizeBar::RIGHT] = LLUICtrlFactory::create<LLResizeBar>(p);
    addChild( mResizeBar[LLResizeBar::RIGHT] );

    p.name("resizebar_bottom");
    p.min_size(mMinHeight);
    p.side(LLResizeBar::BOTTOM);
    mResizeBar[LLResizeBar::BOTTOM] = LLUICtrlFactory::create<LLResizeBar>(p);
    addChild( mResizeBar[LLResizeBar::BOTTOM] );

    // Resize handles (corners)
    LLResizeHandle::Params handle_p;
    // handles must not be mouse-opaque, otherwise they block hover events
    // to other buttons like the close box. JC
    handle_p.mouse_opaque(false);
    handle_p.min_width(mMinWidth);
    handle_p.min_height(mMinHeight);
    handle_p.corner(LLResizeHandle::RIGHT_BOTTOM);
    mResizeHandle[0] = LLUICtrlFactory::create<LLResizeHandle>(handle_p);
    addChild(mResizeHandle[0]);

    handle_p.corner(LLResizeHandle::RIGHT_TOP);
    mResizeHandle[1] = LLUICtrlFactory::create<LLResizeHandle>(handle_p);
    addChild(mResizeHandle[1]);

    handle_p.corner(LLResizeHandle::LEFT_BOTTOM);
    mResizeHandle[2] = LLUICtrlFactory::create<LLResizeHandle>(handle_p);
    addChild(mResizeHandle[2]);

    handle_p.corner(LLResizeHandle::LEFT_TOP);
    mResizeHandle[3] = LLUICtrlFactory::create<LLResizeHandle>(handle_p);
    addChild(mResizeHandle[3]);

    layoutResizeCtrls();
}

void LLFloater::layoutResizeCtrls()
{
    LLRect rect;

    // Resize bars (sides)
    const S32 RESIZE_BAR_THICKNESS = 3;
    rect = LLRect( 0, getRect().getHeight(), RESIZE_BAR_THICKNESS, 0);
    mResizeBar[LLResizeBar::LEFT]->setRect(rect);

    rect = LLRect( 0, getRect().getHeight(), getRect().getWidth(), getRect().getHeight() - RESIZE_BAR_THICKNESS);
    mResizeBar[LLResizeBar::TOP]->setRect(rect);

    rect = LLRect(getRect().getWidth() - RESIZE_BAR_THICKNESS, getRect().getHeight(), getRect().getWidth(), 0);
    mResizeBar[LLResizeBar::RIGHT]->setRect(rect);

    rect = LLRect(0, RESIZE_BAR_THICKNESS, getRect().getWidth(), 0);
    mResizeBar[LLResizeBar::BOTTOM]->setRect(rect);

    // Resize handles (corners)
    rect = LLRect( getRect().getWidth() - RESIZE_HANDLE_WIDTH, RESIZE_HANDLE_HEIGHT, getRect().getWidth(), 0);
    mResizeHandle[0]->setRect(rect);

    rect = LLRect( getRect().getWidth() - RESIZE_HANDLE_WIDTH, getRect().getHeight(), getRect().getWidth(), getRect().getHeight() - RESIZE_HANDLE_HEIGHT);
    mResizeHandle[1]->setRect(rect);

    rect = LLRect( 0, RESIZE_HANDLE_HEIGHT, RESIZE_HANDLE_WIDTH, 0 );
    mResizeHandle[2]->setRect(rect);

    rect = LLRect( 0, getRect().getHeight(), RESIZE_HANDLE_WIDTH, getRect().getHeight() - RESIZE_HANDLE_HEIGHT );
    mResizeHandle[3]->setRect(rect);
}

void LLFloater::enableResizeCtrls(bool enable, bool width, bool height)
{
    mResizeBar[LLResizeBar::LEFT]->setVisible(enable && width);
    mResizeBar[LLResizeBar::LEFT]->setEnabled(enable && width);

    mResizeBar[LLResizeBar::TOP]->setVisible(enable && height);
    mResizeBar[LLResizeBar::TOP]->setEnabled(enable && height);

    mResizeBar[LLResizeBar::RIGHT]->setVisible(enable && width);
    mResizeBar[LLResizeBar::RIGHT]->setEnabled(enable && width);

    mResizeBar[LLResizeBar::BOTTOM]->setVisible(enable && height);
    mResizeBar[LLResizeBar::BOTTOM]->setEnabled(enable && height);

    for (S32 i = 0; i < 4; ++i)
    {
        mResizeHandle[i]->setVisible(enable && width && height);
        mResizeHandle[i]->setEnabled(enable && width && height);
    }
}

void LLFloater::destroy()
{
    // LLFloaterReg should be synchronized with "dead" floater to avoid returning dead instance before
    // it was deleted via LLMortician::updateClass(). See EXT-8458.
    LLFloaterReg::removeInstance(mInstanceName, mKey);
    die();
}

// virtual
LLFloater::~LLFloater()
{
    if (!isDead())
    {
        // If it's dead, instance is supposed to be already removed, and
        // in case of single instance we can remove new one by accident
        LLFloaterReg::removeInstance(mInstanceName, mKey);
    }

    if( gFocusMgr.childHasKeyboardFocus(this))
    {
        // Just in case we might still have focus here, release it.
        releaseFocus();
    }

    // This is important so that floaters with persistent rects (i.e., those
    // created with rect control rather than an LLRect) are restored in their
    // correct, non-minimized positions.
    setMinimized( false );

    delete mDragHandle;
    for (S32 i = 0; i < 4; i++)
    {
        delete mResizeBar[i];
        delete mResizeHandle[i];
    }

    setVisible(false); // We're not visible if we're destroyed
    storeVisibilityControl();
    storeDockStateControl();
    delete mMinimizeSignal;
}

void LLFloater::storeRectControl()
{
    // <FS:Zi> Do not store rect when attached to another floater
    //if (!mRectControl.empty())
    if (mTornOff && !mRectControl.empty())
    // </FS:Zi>
    {
        getControlGroup()->setRect( mRectControl, getRect() );
    }
    if (!mPosXControl.empty() && mPositioning == LLFloaterEnums::POSITIONING_RELATIVE)
    {
        getControlGroup()->setF32( mPosXControl, mPosition.mX );
    }
    if (!mPosYControl.empty() && mPositioning == LLFloaterEnums::POSITIONING_RELATIVE)
    {
        getControlGroup()->setF32( mPosYControl, mPosition.mY );
    }
}

void LLFloater::storeVisibilityControl()
{
    if( !sQuitting && mVisibilityControl.size() > 1 )
    {
        // <FS:Zi> Make sure that hosted floaters always save "not visible", so they won't
        //         pull up their host on restart
        //getControlGroup()->setBOOL( mVisibilityControl, getVisible() );
        bool visible = false;
        if (mTornOff)
        {
            visible = getVisible();
        }
        getControlGroup()->setBOOL( mVisibilityControl, visible );
        // </FS:Zi>
    }
}

void LLFloater::storeDockStateControl()
{
    if( !sQuitting && mDocStateControl.size() > 1 )
    {
        getControlGroup()->setBOOL( mDocStateControl, isDocked() );
    }
}

// static
std::string LLFloater::getControlName(const std::string& name, const LLSD& key)
{
    std::string ctrl_name = name;

    // Add the key to the control name if appropriate.
    if (key.isString() && !key.asString().empty())
    {
        ctrl_name += "_" + key.asString();
    }

    return ctrl_name;
}

// static
LLControlGroup* LLFloater::getControlGroup()
{
    // Floater size, position, visibility, etc are saved in per-account settings.
    return LLUI::getInstance()->mSettingGroups["account"];
}

void LLFloater::setVisible( bool visible )
{
    LLPanel::setVisible(visible); // calls onVisibilityChange()
    if( visible && mFirstLook )
    {
        mFirstLook = false;
    }

    if( !visible )
    {
        LLUI::getInstance()->removePopup(this);

        if( gFocusMgr.childHasMouseCapture( this ) )
        {
            gFocusMgr.setMouseCapture(NULL);
        }
    }

    for(handle_set_iter_t dependent_it = mDependents.begin();
        dependent_it != mDependents.end(); )
    {
        LLFloater* floaterp = dependent_it->get();

        if (floaterp)
        {
            floaterp->setVisible(visible);
        }
        ++dependent_it;
    }

    storeVisibilityControl();
}


void LLFloater::setIsSingleInstance(bool is_single_instance)
{
    mSingleInstance = is_single_instance;
    if (!mIsReuseInitialized)
    {
        mReuseInstance = is_single_instance; // reuse single-instance floaters by default
    }
}


// virtual
void LLFloater::onVisibilityChange ( bool new_visibility )
{
    if (new_visibility)
    {
        if (getHost())
            getHost()->setFloaterFlashing(this, false);
    }
    LLPanel::onVisibilityChange ( new_visibility );
}

void LLFloater::openFloater(const LLSD& key)
{
    LL_INFOS() << "Opening floater " << getName() << " full path: " << getPathname() << LL_ENDL;

    LLViewerEventRecorder::instance().logVisibilityChange( getPathname(), getName(), true,"floater"); // Last param is event subtype or empty string

    mKey = key; // in case we need to open ourselves again

    if (getSoundFlags() != SILENT
    // don't play open sound for hosted (tabbed) windows
        && !getHost()
        && !getFloaterHost()
        && (!getVisible() || isMinimized()))
    {
        // <FS:PP> UI Sounds connection
        if (getName() == "script_floater")
        {
            make_ui_sound("UISndScriptFloaterOpen");
        }
        else
        {
            make_ui_sound("UISndWindowOpen");
        }
        // </FS:PP>
    }

    //RN: for now, we don't allow rehosting from one multifloater to another
    // just need to fix the bugs
    if (getFloaterHost() != NULL && getHost() == NULL)
    {
        // needs a host
        // only select tabs if window they are hosted in is visible
        getFloaterHost()->addFloater(this, getFloaterHost()->getVisible());
    }

    if (getHost() != NULL)
    {
        getHost()->setMinimized(false);
        getHost()->setVisibleAndFrontmost(mAutoFocus && !getIsChrome());
        getHost()->showFloater(this);
        // <FS:Zi> Make sure the floater knows it's not torn off
        mTornOff = false;
    }
    else
    {
        // <FS:Zi> Make sure the floater knows it's torn off
        mTornOff = true;

        LLFloater* floater_to_stack = LLFloaterReg::getLastFloaterInGroup(mInstanceName);
        if (!floater_to_stack)
        {
            floater_to_stack = LLFloaterReg::getLastFloaterCascading();
        }
        applyControlsAndPosition(floater_to_stack);
        setMinimized(false);
        setVisibleAndFrontmost(mAutoFocus && !getIsChrome());
    }

    mOpenSignal(this, key);
    onOpen(key);

    dirtyRect();
}

void LLFloater::closeFloater(bool app_quitting)
{
    // <FS:PP> FIRE-10373 / BUG-6437: UISndWindowClose played if an online or offline notification toast is still open for the same person
    // LL_INFOS() << "Closing floater " << getName() << LL_ENDL;
    // LLViewerEventRecorder::instance().logVisibilityChange( getPathname(), getName(), false,"floater"); // Last param is event subtype or empty string
    std::string floaterName = getName();
    LL_INFOS() << "Closing floater " << floaterName << LL_ENDL;
    LLViewerEventRecorder::instance().logVisibilityChange( getPathname(), floaterName, false,"floater"); // Last param is event subtype or empty string
    // </FS:PP>
    if (app_quitting)
    {
        LLFloater::sQuitting = true;
    }

    // Always unminimize before trying to close.
    // Most of the time the user will never see this state.
    setMinimized(false);

    if (canClose())
    {
        if (getHost())
        {
            ((LLMultiFloater*)getHost())->removeFloater(this);
            gFloaterView->addChild(this);
        }

        if (getSoundFlags() != SILENT
            && getVisible()
            && !getHost()
            && !app_quitting
            && floaterName != "toast") // <FS:PP> FIRE-10373 / BUG-6437
        {
            make_ui_sound("UISndWindowClose");
        }

        gFocusMgr.clearLastFocusForGroup(this);

            if (hasFocus())
            {
                // Do this early, so UI controls will commit before the
                // window is taken down.
                releaseFocus();

                // give focus to dependee floater if it exists, and we had focus first
                if (isDependent())
                {
                    LLFloater* dependee = mDependeeHandle.get();
                    if (dependee && !dependee->isDead())
                    {
                        dependee->setFocus(true);
                    }
                }
            }


        //If floater is a dependent, remove it from parent (dependee)
        LLFloater* dependee = mDependeeHandle.get();
        if (dependee)
        {
            dependee->removeDependentFloater(this);
        }

        // now close dependent floater
        while(mDependents.size() > 0)
        {
            handle_set_iter_t dependent_it = mDependents.begin();
            LLFloater* floaterp = dependent_it->get();
            // normally removeDependentFloater will do this, but in
            // case floaterp is somehow invalid or orphaned, erase now
            mDependents.erase(dependent_it);
            if (floaterp)
            {
                floaterp->mDependeeHandle = LLHandle<LLFloater>();
                floaterp->closeFloater(app_quitting);
            }
        }

        cleanupHandles();

        dirtyRect();

        // Close callbacks
        onClose(app_quitting);
        mCloseSignal(this, LLSD(app_quitting));

        // Hide or Destroy
        if (mSingleInstance)
        {
            // Hide the instance
            if (getHost())
            {
                getHost()->setVisible(false);
            }
            else
            {
                setVisible(false);
                if (!mReuseInstance)
                {
                    destroy();
                }
            }
        }
        else
        {
            setVisible(false); // hide before destroying (so onVisibilityChange() gets called)
            if (!mReuseInstance)
            {
                destroy();
            }
        }
    }
}

/*virtual*/
void LLFloater::closeHostedFloater()
{
    // When toggling *visibility*, close the host instead of the floater when hosted
    if (getHost())
    {
        getHost()->closeFloater();
    }
    else
    {
        closeFloater();
    }
}

/*virtual*/
void LLFloater::reshape(S32 width, S32 height, bool called_from_parent)
{
    LLPanel::reshape(width, height, called_from_parent);
}

// virtual
void LLFloater::translate(S32 x, S32 y)
{
    LLView::translate(x, y);

    if (!mTranslateWithDependents || mDependents.empty())
        return;

    for (const LLHandle<LLFloater>& handle : mDependents)
    {
        LLFloater* floater = handle.get();
        if (floater && floater->getSnapTarget() == getHandle())
        {
            floater->LLView::translate(x, y);
        }
    }
}

void LLFloater::releaseFocus()
{
    LLUI::getInstance()->removePopup(this);

    setFocus(false);

    if( gFocusMgr.childHasMouseCapture( this ) )
    {
        gFocusMgr.setMouseCapture(NULL);
    }
}


void LLFloater::setResizeLimits( S32 min_width, S32 min_height )
{
    mMinWidth = min_width;
    mMinHeight = min_height;

    for( S32 i = 0; i < 4; i++ )
    {
        if( mResizeBar[i] )
        {
            if (i == LLResizeBar::LEFT || i == LLResizeBar::RIGHT)
            {
                mResizeBar[i]->setResizeLimits( min_width, S32_MAX );
            }
            else
            {
                mResizeBar[i]->setResizeLimits( min_height, S32_MAX );
            }
        }
        if( mResizeHandle[i] )
        {
            mResizeHandle[i]->setResizeLimits( min_width, min_height );
        }
    }
}


void LLFloater::center()
{
    if(getHost())
    {
        // hosted floaters can't move
        return;
    }
    centerWithin(gFloaterView->getRect());
}

LLMultiFloater* LLFloater::getHost()
{
    return (LLMultiFloater*)mHostHandle.get();
}

void LLFloater::applyControlsAndPosition(LLFloater* other)
{
    // <FS:Zi> Don't apply dock state and forget about the undocked values
    // AH: Apply the dock state before applying the rect control. applyDockState
    //     will call SetDocked with pop_on_undock = true and translate the floater
    //     by 12px on the y-axis, so we have to apply the rect control after that
    //     to have it in the right position.
    //  if (!applyDockState())
    applyDockState();
    // </FS:Zi>
    {
        if (!applyRectControl())
        {
            applyPositioning(other, true);
        }
    }
}

bool LLFloater::applyRectControl()
{
    bool saved_rect = false;

    LLRect screen_rect = calcScreenRect();
    mPosition = LLCoordGL(screen_rect.getCenterX(), screen_rect.getCenterY()).convert();

    LLFloater* last_in_group = LLFloaterReg::getLastFloaterInGroup(mInstanceName);
    if (last_in_group && last_in_group != this)
    {
        // <FS:Ansariel> Open other floaters in group in the stored size (this
        //               is basically taken from the else-branch below)
        if (!mRectControl.empty())
        {
            // If we have a saved rect, use it
            const LLRect& rect = getControlGroup()->getRect(mRectControl);
            if (rect.notEmpty() && mResizable)
            {
                reshape(llmax(mMinWidth, rect.getWidth()), llmax(mMinHeight, rect.getHeight()));
            }
        }
        // </FS:Ansariel>

        // other floaters in our group, position ourselves relative to them and don't save the rect
        if (mDefaultRectForGroup)
        {
            mRectControl.clear();
        }
        mPositioning = LLFloaterEnums::POSITIONING_CASCADE_GROUP;
    }
    else
    {
        bool rect_specified = false;
        if (!mRectControl.empty())
        {
            // If we have a saved rect, use it
            const LLRect& rect = getControlGroup()->getRect(mRectControl);
            if (rect.notEmpty()) saved_rect = true;
            if (saved_rect)
            {
                setOrigin(rect.mLeft, rect.mBottom);

                if (mResizable)
                {
                    reshape(llmax(mMinWidth, rect.getWidth()), llmax(mMinHeight, rect.getHeight()));
                }
                mPositioning = LLFloaterEnums::POSITIONING_RELATIVE;
                LLRect screen_rect = calcScreenRect();
                mPosition = LLCoordGL(screen_rect.getCenterX(), screen_rect.getCenterY()).convert();
                rect_specified = true;
            }
        }

        LLControlVariablePtr x_control = getControlGroup()->getControl(mPosXControl);
        LLControlVariablePtr y_control = getControlGroup()->getControl(mPosYControl);
        if (x_control.notNull()
            && y_control.notNull()
            && !x_control->isDefault()
            && !y_control->isDefault())
        {
            mPosition.mX = (LL_COORD_FLOATER::value_t)x_control->getValue().asReal();
            mPosition.mY = (LL_COORD_FLOATER::value_t)y_control->getValue().asReal();
            mPositioning = LLFloaterEnums::POSITIONING_RELATIVE;
            applyRelativePosition();

            saved_rect = true;
        }
        else if ((mDefaultRelativeX != 0) && (mDefaultRelativeY != 0))
        {
            mPosition.mX = mDefaultRelativeX;
            mPosition.mY = mDefaultRelativeY;
            mPositioning = LLFloaterEnums::POSITIONING_RELATIVE;
            applyRelativePosition();

            saved_rect = true;
        }

        // remember updated position
        if (rect_specified)
        {
            storeRectControl();
        }
    }

    if (saved_rect)
    {
        // propagate any derived positioning data back to settings file
        storeRectControl();
    }


    return saved_rect;
}

bool LLFloater::applyDockState()
{
    bool docked = false;

    if (mDocStateControl.size() > 1)
    {
        docked = getControlGroup()->getBOOL(mDocStateControl);
        setDocked(docked);
    }

    return docked;
}

void LLFloater::applyPositioning(LLFloater* other, bool on_open)
{
    // Otherwise position according to the positioning code
    switch (mPositioning)
    {
    case LLFloaterEnums::POSITIONING_CENTERED:
        center();
        break;

    case LLFloaterEnums::POSITIONING_SPECIFIED:
        break;

    case LLFloaterEnums::POSITIONING_CASCADING:
        if (!on_open)
        {
            applyRelativePosition();
        }
        // fall through
    case LLFloaterEnums::POSITIONING_CASCADE_GROUP:
        if (on_open)
        {
            if (other != NULL && other != this)
            {
                stackWith(*other);
            }
            else
            {
                static const U32 CASCADING_FLOATER_HOFFSET = 0;
                static const U32 CASCADING_FLOATER_VOFFSET = 0;

                const LLRect& snap_rect = gFloaterView->getSnapRect();

                const S32 horizontal_offset = CASCADING_FLOATER_HOFFSET;
                const S32 vertical_offset = snap_rect.getHeight() - CASCADING_FLOATER_VOFFSET;

                S32 rect_height = getRect().getHeight();
                setOrigin(horizontal_offset, vertical_offset - rect_height);

                translate(snap_rect.mLeft, snap_rect.mBottom);
            }
            setFollows(FOLLOWS_TOP | FOLLOWS_LEFT);
        }
        break;

    case LLFloaterEnums::POSITIONING_RELATIVE:
        {
            applyRelativePosition();

            break;
        }
    default:
        // Do nothing
        break;
    }
}

void LLFloater::applyTitle()
{
    if (!mDragHandle)
    {
        return;
    }

    if (isMinimized() && !mShortTitle.empty())
    {
        mDragHandle->setTitle( mShortTitle );
    }
    else
    {
        mDragHandle->setTitle ( mTitle );
    }

    if (getHost())
    {
        getHost()->updateFloaterTitle(this);
    }
}

std::string LLFloater::getCurrentTitle() const
{
    return mDragHandle ? mDragHandle->getTitle() : LLStringUtil::null;
}

void LLFloater::setTitle( const std::string& title )
{
    mTitle = title;
    applyTitle();
}

std::string LLFloater::getTitle() const
{
    if (mTitle.empty())
    {
        return mDragHandle ? mDragHandle->getTitle() : LLStringUtil::null;
    }
    else
    {
        return mTitle;
    }
}

void LLFloater::setShortTitle( const std::string& short_title )
{
    mShortTitle = short_title;
    applyTitle();
}

std::string LLFloater::getShortTitle() const
{
    if (mShortTitle.empty())
    {
        return mDragHandle ? mDragHandle->getTitle() : LLStringUtil::null;
    }
    else
    {
        return mShortTitle;
    }
}

bool LLFloater::canSnapTo(const LLView* other_view)
{
    if (NULL == other_view)
    {
        LL_WARNS() << "other_view is NULL" << LL_ENDL;
        return false;
    }

    if (other_view != getParent())
    {
        const LLFloater* other_floaterp = dynamic_cast<const LLFloater*>(other_view);
        if (other_floaterp
            && other_floaterp->getSnapTarget() == getHandle()
            && mDependents.find(other_floaterp->getHandle()) != mDependents.end())
        {
            // this is a dependent that is already snapped to us, so don't snap back to it
            return false;
        }
    }

    return LLPanel::canSnapTo(other_view);
}

void LLFloater::setSnappedTo(const LLView* snap_view)
{
    if (!snap_view || snap_view == getParent())
    {
        clearSnapTarget();
    }
    else
    {
        //RN: assume it's a floater as it must be a sibling to our parent floater
        const LLFloater* floaterp = dynamic_cast<const LLFloater*>(snap_view);
        if (floaterp)
        {
            setSnapTarget(floaterp->getHandle());
        }
    }
}

void LLFloater::handleReshape(const LLRect& new_rect, bool by_user)
{
    const LLRect old_rect = getRect();
    LLView::handleReshape(new_rect, by_user);

    if (by_user && !getHost())
    {
        LLFloaterView * floaterVp = dynamic_cast<LLFloaterView*>(getParent());
        if (floaterVp)
        {
            floaterVp->adjustToFitScreen(this, !isMinimized());
        }
    }

    // if not minimized, adjust all snapped dependents to new shape
    if (!isMinimized())
    {
        if (by_user)
        {
            if (isDocked())
            {
                setDocked( false, false);
            }
        mPositioning = LLFloaterEnums::POSITIONING_RELATIVE;
        LLRect screen_rect = calcScreenRect();
        mPosition = LLCoordGL(screen_rect.getCenterX(), screen_rect.getCenterY()).convert();
        }
        storeRectControl();

        // gather all snapped dependents
        for(handle_set_iter_t dependent_it = mDependents.begin();
            dependent_it != mDependents.end(); ++dependent_it)
        {
            LLFloater* floaterp = dependent_it->get();
            // is a dependent snapped to us?
            if (floaterp && floaterp->getSnapTarget() == getHandle())
            {
                S32 delta_x = 0;
                S32 delta_y = 0;

                // take translation of dependee floater into account
                delta_x += new_rect.mLeft - old_rect.mLeft;
                delta_y += new_rect.mBottom - old_rect.mBottom;

                // check to see if it snapped to right or top, and move if dependee floater is resizing
                LLRect dependent_rect = floaterp->getRect();
                if ((dependent_rect.mLeft - getRect().mLeft >= old_rect.getWidth() || // dependent on my right?
                     dependent_rect.mRight == getRect().mLeft + old_rect.getWidth()) // dependent aligned with my right
                    && dependent_rect.mBottom <= old_rect.mTop + 1)
                {
                    // was snapped directly onto right side or aligned with it
                    delta_x += new_rect.getWidth() - old_rect.getWidth();

                    // make sure dependent still touches floater and din't go too high,
                    // it can go over edge, but should't detach completely
                    if (delta_y > 0
                        && dependent_rect.mBottom + delta_y > new_rect.mTop)
                    {
                        delta_y = llmax(new_rect.mTop - dependent_rect.mBottom, 0);
                    }
                }
                else if (dependent_rect.mRight == old_rect.mLeft)
                {
                    // make sure dependent still touches floater and don't go too high
                    if (delta_y > 0
                        && dependent_rect.mBottom <= old_rect.mTop
                        && dependent_rect.mBottom + delta_y > new_rect.mTop)
                    {
                        delta_y = llmax(new_rect.mTop - dependent_rect.mBottom, 0);
                    }
                }

                if ((dependent_rect.mBottom - getRect().mBottom >= old_rect.getHeight() ||
                     dependent_rect.mTop == getRect().mBottom + old_rect.getHeight())
                    && dependent_rect.mLeft <= old_rect.mRight + 1)
                {
                    // was snapped directly onto top side or aligned with it
                    delta_y += new_rect.getHeight() - old_rect.getHeight();

                    // make sure dependent still touches floater
                    // and din't go too far to the right
                    if (delta_x > 0
                        && dependent_rect.mLeft + delta_x > new_rect.mRight)
                    {
                        delta_x = llmax(new_rect.mRight - dependent_rect.mLeft, 0);
                    }
                }
                else if (dependent_rect.mTop == old_rect.mBottom)
                {
                    // make sure dependent still touches floater and don't go too far to the right
                    if (delta_x > 0
                        && dependent_rect.mLeft <= old_rect.mRight
                        && dependent_rect.mLeft + delta_x > new_rect.mRight)
                    {
                        delta_x = llmax(new_rect.mRight - dependent_rect.mLeft, 0);
                    }
                }

                dependent_rect.translate(delta_x, delta_y);
                floaterp->setShape(dependent_rect, by_user);
            }
        }
    }
    else
    {
        // If minimized, and origin has changed, set
        // mHasBeenDraggedWhileMinimized to true
        if ((new_rect.mLeft != old_rect.mLeft) ||
            (new_rect.mBottom != old_rect.mBottom))
        {
            mHasBeenDraggedWhileMinimized = true;
        }
    }
}

void LLFloater::setMinimized(bool minimize)
{
    const LLFloater::Params& default_params = LLFloater::getDefaultParams();
    S32 floater_header_size = default_params.header_height;
    static LLUICachedControl<S32> minimized_width ("UIMinimizedWidth", 0);

    if (minimize == mMinimized) return;

    if (mMinimizeSignal)
    {
        (*mMinimizeSignal)(this, LLSD(minimize));
    }

    if (minimize)
    {
        // minimized flag should be turned on before release focus
        mMinimized = true;
        mExpandedRect = getRect();

        // If the floater has been dragged while minimized in the
        // past, then locate it at its previous minimized location.
        // Otherwise, ask the view for a minimize position.
        if (mHasBeenDraggedWhileMinimized)
        {
            setOrigin(mPreviousMinimizedLeft, mPreviousMinimizedBottom);
        }
        else
        {
            S32 left, bottom;
            gFloaterView->getMinimizePosition(&left, &bottom);
            setOrigin( left, bottom );
        }

        if (mButtonsEnabled[BUTTON_MINIMIZE])
        {
            mButtonsEnabled[BUTTON_MINIMIZE] = false;
            mButtonsEnabled[BUTTON_RESTORE] = true;
        }

        setBorderVisible(true);

        for(handle_set_iter_t dependent_it = mDependents.begin();
            dependent_it != mDependents.end();
            ++dependent_it)
        {
            LLFloater* floaterp = dependent_it->get();
            if (floaterp)
            {
                if (floaterp->isMinimizeable())
                {
                    floaterp->setMinimized(true);
                }
                else if (!floaterp->isMinimized())
                {
                    floaterp->setVisible(false);
                }
            }
        }

        // Lose keyboard focus when minimized
        releaseFocus();

        for (S32 i = 0; i < 4; i++)
        {
            if (mResizeBar[i] != NULL)
            {
                mResizeBar[i]->setEnabled(false);
            }
            if (mResizeHandle[i] != NULL)
            {
                mResizeHandle[i]->setEnabled(false);
            }
        }

        // Reshape *after* setting mMinimized
        reshape( minimized_width, floater_header_size, true);
    }
    else
    {
        // If this window has been dragged while minimized (at any time),
        // remember its position for the next time it's minimized.
        if (mHasBeenDraggedWhileMinimized)
        {
            const LLRect& currentRect = getRect();
            mPreviousMinimizedLeft = currentRect.mLeft;
            mPreviousMinimizedBottom = currentRect.mBottom;
        }

        setOrigin( mExpandedRect.mLeft, mExpandedRect.mBottom );
        if (mButtonsEnabled[BUTTON_RESTORE])
        {
            mButtonsEnabled[BUTTON_MINIMIZE] = true;
            mButtonsEnabled[BUTTON_RESTORE] = false;
        }

        // show dependent floater
        for(handle_set_iter_t dependent_it = mDependents.begin();
            dependent_it != mDependents.end();
            ++dependent_it)
        {
            LLFloater* floaterp = dependent_it->get();
            if (floaterp)
            {
                floaterp->setMinimized(false);
                floaterp->setVisible(true);
            }
        }

        for (S32 i = 0; i < 4; i++)
        {
            if (mResizeBar[i] != NULL)
            {
                mResizeBar[i]->setEnabled(isResizable());
            }
            if (mResizeHandle[i] != NULL)
            {
                mResizeHandle[i]->setEnabled(isResizable());
            }
        }

        mMinimized = false;
        setFrontmost();
        // Reshape *after* setting mMinimized
        reshape( mExpandedRect.getWidth(), mExpandedRect.getHeight(), true );
    }

    make_ui_sound("UISndWindowClose");
    updateTitleButtons();
    applyTitle ();
}

void LLFloater::setFocus( bool b )
{
    if (b && getIsChrome())
    {
        return;
    }
    LLView* last_focus = gFocusMgr.getLastFocusForGroup(this);
    // a descendent already has focus
    bool child_had_focus = hasFocus();

    // give focus to first valid descendent
    LLPanel::setFocus(b);

    if (b)
    {
        // only push focused floaters to front of stack if not in midst of ctrl-tab cycle
        LLFloaterView * parent = dynamic_cast<LLFloaterView *>(getParent());
        if (!getHost() && parent && !parent->getCycleMode())
        {
            if (!isFrontmost())
            {
                setFrontmost();
            }
        }

        // when getting focus, delegate to last descendent which had focus
        if (last_focus && !child_had_focus &&
            last_focus->isInEnabledChain() &&
            last_focus->isInVisibleChain())
        {
            // *FIX: should handle case where focus doesn't stick
            last_focus->setFocus(true);
        }
    }
    updateTransparency(b ? TT_ACTIVE : TT_INACTIVE);
}

// virtual
void LLFloater::setRect(const LLRect &rect)
{
    LLPanel::setRect(rect);
    layoutDragHandle();
    layoutResizeCtrls();
}

// virtual
void LLFloater::setIsChrome(bool is_chrome)
{
    // chrome floaters don't take focus at all
    if (is_chrome)
    {
        // remove focus if we're changing to chrome
        setFocus(false);
        // can't Ctrl-Tab to "chrome" floaters
        setFocusRoot(false);
        mButtons[BUTTON_CLOSE]->setToolTip(LLStringExplicit(getButtonTooltip(Params(), BUTTON_CLOSE, is_chrome)));
    }

    LLPanel::setIsChrome(is_chrome);
}

// Change the draw style to account for the foreground state.
void LLFloater::setForeground(bool front)
{
    if (front != mForeground)
    {
        mForeground = front;
        if (mDragHandle)
            mDragHandle->setForeground( front );

        if (!front)
        {
            releaseFocus();
        }

        setBackgroundOpaque( front );
    }
}

void LLFloater::cleanupHandles()
{
    // remove handles to non-existent dependents
    for(handle_set_iter_t dependent_it = mDependents.begin();
        dependent_it != mDependents.end(); )
    {
        LLFloater* floaterp = dependent_it->get();
        if (!floaterp)
        {
            dependent_it = mDependents.erase(dependent_it);
        }
        else
        {
            ++dependent_it;
        }
    }
}

void LLFloater::setHost(LLMultiFloater* host)
{
    if (mHostHandle.isDead() && host)
    {
        // make buttons smaller for hosted windows to differentiate from parent
        mButtonScale = 0.9f;

        // add tear off button
        if (mCanTearOff)
        {
            mButtonsEnabled[BUTTON_TEAR_OFF] = true;
        }
    }
    else if (!mHostHandle.isDead() && !host)
    {
        mButtonScale = 1.f;
        //mButtonsEnabled[BUTTON_TEAR_OFF] = false;
    }
    if (host)
    {
        mHostHandle = host->getHandle();
        mLastHostHandle = host->getHandle();
    }
    else
    {
        mHostHandle.markDead();
    }

    updateTitleButtons();
}

void LLFloater::moveResizeHandlesToFront()
{
    for( S32 i = 0; i < 4; i++ )
    {
        if( mResizeBar[i] )
        {
            sendChildToFront(mResizeBar[i]);
        }
    }

    for( S32 i = 0; i < 4; i++ )
    {
        if( mResizeHandle[i] )
        {
            sendChildToFront(mResizeHandle[i]);
        }
    }
}

/*virtual*/
bool LLFloater::isFrontmost()
{
    LLFloaterView* floater_view = getParentByType<LLFloaterView>();
    return getVisible()
            && (floater_view
                && floater_view->getFrontmost() == this);
}

void LLFloater::addDependentFloater(LLFloater* floaterp, bool reposition, bool resize)
{
    mDependents.insert(floaterp->getHandle());
    floaterp->mDependeeHandle = getHandle();

    if (reposition)
    {
        LLRect rect = gFloaterView->findNeighboringPosition(this, floaterp);
        if (resize)
        {
            const LLRect& base = getRect();
            if (rect.mTop == base.mTop)
                rect.mBottom = base.mBottom;
            else if (rect.mLeft == base.mLeft)
                rect.mRight = base.mRight;
            floaterp->reshape(rect.getWidth(), rect.getHeight(), false);
        }
        floaterp->setRect(rect);
        floaterp->setSnapTarget(getHandle());
    }
    gFloaterView->adjustToFitScreen(floaterp, false, true);
    if (floaterp->isFrontmost())
    {
        // make sure to bring self and sibling floaters to front
        gFloaterView->bringToFront(floaterp, floaterp->getAutoFocus() && !getIsChrome());
    }
}

void LLFloater::addDependentFloater(LLHandle<LLFloater> dependent, bool reposition, bool resize)
{
    LLFloater* dependent_floaterp = dependent.get();
    if(dependent_floaterp)
    {
        addDependentFloater(dependent_floaterp, reposition, resize);
    }
}

void LLFloater::removeDependentFloater(LLFloater* floaterp)
{
    mDependents.erase(floaterp->getHandle());
    floaterp->mDependeeHandle = LLHandle<LLFloater>();
}

// <FS:Ansariel> Fix floater relocation
//void LLFloater::fitWithDependentsOnScreen(const LLRect& left, const LLRect& bottom, const LLRect& right, const LLRect& constraint, S32 min_overlap_pixels)
void LLFloater::fitWithDependentsOnScreen(const LLRect& left, const LLRect& bottom, const LLRect& right, const LLRect& chatbar, const LLRect& utilitybar, const LLRect& constraint, S32 min_overlap_pixels)
// </FS:Ansariel>
{
    LLRect total_rect = getRect();

    for (const LLHandle<LLFloater>& handle : mDependents)
    {
        LLFloater* floater = handle.get();
        if (floater && floater->getSnapTarget() == getHandle())
        {
            total_rect.unionWith(floater->getRect());
        }
    }

    S32 delta_left = left.notEmpty() ? left.mRight - total_rect.mRight : 0;
    S32 delta_bottom = bottom.notEmpty() ? bottom.mTop - total_rect.mTop : 0;
    S32 delta_right = right.notEmpty() ? right.mLeft - total_rect.mLeft : 0;
    // <FS:Ansariel> Prevent floaters being dragged under main chat bar
    S32 delta_bottom_chatbar = chatbar.notEmpty() ? chatbar.mTop - total_rect.mTop : 0;
    S32 delta_utility_bar = utilitybar.notEmpty() ? utilitybar.mTop - total_rect.mTop : 0;

    // <FS:Ansariel> Fix floater relocation for vertical toolbars; Only header guarantees that floater can be dragged!
    S32 header_height = getHeaderHeight();

    // move floater with dependings fully onscreen
    mTranslateWithDependents = true;
    if (translateRectIntoRect(total_rect, constraint, min_overlap_pixels))
    {
        clearSnapTarget();
    }
    // <FS:Ansariel> Fix floater relocation for vertical toolbars; Only header guarantees that floater can be dragged!
    //else if (delta_left > 0 && total_rect.mTop < left.mTop && total_rect.mBottom > left.mBottom)
    else if (delta_left > 0 && total_rect.mTop < left.mTop && (total_rect.mTop - header_height) > left.mBottom)
    // </FS:Ansariel>
    {
        translate(delta_left, 0);
    }
    // <FS:Ansariel> Prevent floaters being dragged under main chat bar
    //else if (delta_bottom > 0 && total_rect.mLeft > bottom.mLeft && total_rect.mRight < bottom.mRight)
    else if (delta_bottom > 0 && ((total_rect.mLeft > bottom.mLeft && total_rect.mRight < bottom.mRight) // floater completely within toolbar rect
        || (total_rect.mLeft > bottom.mLeft && total_rect.mLeft < bottom.mRight && bottom.mRight > constraint.mRight) // floater partially within toolbar rect, toolbar bound to right side
        || (delta_bottom_chatbar > 0 && total_rect.mLeft < chatbar.mRight && total_rect.mRight > bottom.mLeft && bottom.mLeft <= chatbar.mRight)) // floater within chatbar and toolbar rect
        )
    // </FS:Ansariel>
    {
        translate(0, delta_bottom);
    }
    // <FS:Ansariel> Fix floater relocation for vertical toolbars; Only header guarantees that floater can be dragged!
    //else if (delta_right < 0 && total_rect.mTop < right.mTop    && total_rect.mBottom > right.mBottom)
    else if (delta_right < 0 && total_rect.mTop < right.mTop && (total_rect.mTop - header_height) > right.mBottom)
    // </FS:Ansariel>
    {
        translate(delta_right, 0);
    }
    // <FS:Ansariel> Prevent floaters being dragged under main chat bar
    else if (delta_bottom_chatbar > 0 && ((total_rect.mLeft > chatbar.mLeft && total_rect.mRight < chatbar.mRight) // floater completely within chatbar rect
        || (total_rect.mRight > chatbar.mLeft && total_rect.mRight < chatbar.mRight && chatbar.mLeft < constraint.mLeft) // floater partially within chatbar rect, chatbar bound to left side
        || (delta_bottom > 0 && total_rect.mRight > bottom.mLeft && total_rect.mLeft < chatbar.mRight && bottom.mLeft <= chatbar.mRight)) // floater within chatbar and toolbar rect
        )
    {
        translate(0, delta_bottom_chatbar);
    }
    else if (delta_utility_bar > 0 && (total_rect.mLeft > utilitybar.mLeft && total_rect.mRight < utilitybar.mRight))
    {
        // Utility bar on legacy skins
        translate(0, delta_utility_bar);
    }
    // </FS:Ansariel>
    mTranslateWithDependents = false;
}

bool LLFloater::offerClickToButton(S32 x, S32 y, MASK mask, EFloaterButton index)
{
    if( mButtonsEnabled[index] )
    {
        LLButton* my_butt = mButtons[index];
        S32 local_x = x - my_butt->getRect().mLeft;
        S32 local_y = y - my_butt->getRect().mBottom;

        if (
            my_butt->pointInView(local_x, local_y) &&
            my_butt->handleMouseDown(local_x, local_y, mask))
        {
            // the button handled it
            return true;
        }
    }
    return false;
}

bool LLFloater::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
    LLPanel::handleScrollWheel(x,y,clicks);
    return true;//always
}

// virtual
bool LLFloater::handleMouseUp(S32 x, S32 y, MASK mask)
{
    LL_DEBUGS() << "LLFloater::handleMouseUp calling LLPanel (really LLView)'s handleMouseUp (first initialized xui to: " << getPathname() << " )" << LL_ENDL;
    bool handled = LLPanel::handleMouseUp(x,y,mask); // Not implemented in LLPanel so this actually calls LLView
    if (handled) {
        LLViewerEventRecorder::instance().updateMouseEventInfo(x,y,-55,-55,getPathname());
    }
    return handled;
}

// virtual
bool LLFloater::handleMouseDown(S32 x, S32 y, MASK mask)
{
    if( mMinimized )
    {
        // Offer the click to titlebar buttons.
        // Note: this block and the offerClickToButton helper method can be removed
        // because the parent container will handle it for us but we'll keep it here
        // for safety until after reworking the panel code to manage hidden children.
        if(offerClickToButton(x, y, mask, BUTTON_CLOSE)) return true;
        if(offerClickToButton(x, y, mask, BUTTON_RESTORE)) return true;
        if(offerClickToButton(x, y, mask, BUTTON_TEAR_OFF)) return true;
        if(offerClickToButton(x, y, mask, BUTTON_DOCK)) return true;
        // <FS:Ansariel> FIRE-11724: Snooze group chat
        if(offerClickToButton(x, y, mask, BUTTON_SNOOZE)) return true;

        setFrontmost(true, false);
        // Otherwise pass to drag handle for movement
        return mDragHandle->handleMouseDown(x, y, mask);
    }
    else
    {
        bringToFront( x, y );
        bool handled = LLPanel::handleMouseDown( x, y, mask );
        if (handled) {
            LLViewerEventRecorder::instance().updateMouseEventInfo(x,y,-55,-55,getPathname());
        }
        return handled;
    }
}

// virtual
bool LLFloater::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
    bool was_minimized = mMinimized;
    bringToFront( x, y );
    return was_minimized || LLPanel::handleRightMouseDown( x, y, mask );
}

bool LLFloater::handleMiddleMouseDown(S32 x, S32 y, MASK mask)
{
    bringToFront( x, y );
    return LLPanel::handleMiddleMouseDown( x, y, mask );
}


// virtual
bool LLFloater::handleDoubleClick(S32 x, S32 y, MASK mask)
{
    bool was_minimized = mMinimized;
    setMinimized(false);
    return was_minimized || LLPanel::handleDoubleClick(x, y, mask);
}

// virtual
void LLFloater::bringToFront( S32 x, S32 y )
{
    if (getVisible() && pointInView(x, y))
    {
        LLMultiFloater* hostp = getHost();
        if (hostp)
        {
            hostp->showFloater(this);
        }
        else
        {
            LLFloaterView* parent = dynamic_cast<LLFloaterView*>( getParent() );
            if (parent)
            {
                parent->bringToFront(this, !getIsChrome());
            }
        }
    }
}

// virtual
void LLFloater::goneFromFront()
{
    if (mAutoClose)
    {
        closeFloater();
    }
}

// virtual
void LLFloater::setVisibleAndFrontmost(bool take_focus,const LLSD& key)
{
    LLUIUsage::instance().logFloater(getInstanceName());
    LLMultiFloater* hostp = getHost();
    if (hostp)
    {
        hostp->setVisible(true);
        hostp->setFrontmost(take_focus);
    }
    else
    {
        setVisible(true);
        setFrontmost(take_focus);
    }
}

void LLFloater::setFrontmost(bool take_focus, bool restore)
{
    LLMultiFloater* hostp = getHost();
    if (hostp)
    {
        // this will bring the host floater to the front and select
        // the appropriate panel
        hostp->showFloater(this);
    }
    else
    {
        // there are more than one floater view
        // so we need to query our parent directly
        LLFloaterView * parent = dynamic_cast<LLFloaterView*>( getParent() );
        if (parent)
        {
            parent->bringToFront(this, take_focus, restore);
        }

        // Make sure to set the appropriate transparency type (STORM-732).
        updateTransparency(hasFocus() || getIsChrome() ? TT_ACTIVE : TT_INACTIVE);
    }
}

void LLFloater::setCanDock(bool b)
{
    if(b != mCanDock)
    {
        mCanDock = b;
        if(mCanDock)
        {
            mButtonsEnabled[BUTTON_DOCK] = !mDocked;
        }
        else
        {
            mButtonsEnabled[BUTTON_DOCK] = false;
        }
    }
    updateTitleButtons();
}

void LLFloater::setDocked(bool docked, bool pop_on_undock)
{
    if(docked != mDocked && mCanDock)
    {
        mDocked = docked;
        mButtonsEnabled[BUTTON_DOCK] = !mDocked;

        if (mDocked)
        {
            setMinimized(false);
            mPositioning = LLFloaterEnums::POSITIONING_RELATIVE;
        }

        updateTitleButtons();

        storeDockStateControl();
    }

}

// static
void LLFloater::onClickMinimize(LLFloater* self)
{
    if (!self)
        return;
    self->setMinimized( !self->isMinimized() );
}

void LLFloater::onClickTearOff(LLFloater* self)
{
    if (!self)
        return;
    S32 floater_header_size = self->mHeaderHeight;
    LLMultiFloater* host_floater = self->getHost();
    if (host_floater) //Tear off
    {
        LLRect new_rect;
        host_floater->removeFloater(self);
        // reparent to floater view
        gFloaterView->addChild(self);

        self->openFloater(self->getKey());
        if (self->mSaveRect && !self->mRectControl.empty())
        {
            self->applyRectControl();
        }
        else
        {   // only force position for floaters that don't have that data saved
            new_rect.setLeftTopAndSize(host_floater->getRect().mLeft + 5, host_floater->getRect().mTop - floater_header_size - 5, self->getRect().getWidth(), self->getRect().getHeight());
            self->setRect(new_rect);
        }
        gFloaterView->adjustToFitScreen(self, false);
        // give focus to new window to keep continuity for the user
        self->setFocus(true);
        self->setTornOff(true);
    }
    else  //Attach to parent.
    {
        LLMultiFloater* new_host = (LLMultiFloater*)self->mLastHostHandle.get();
        if (new_host)
        {
            if (self->mSaveRect)
            {
                LLRect screen_rect = self->calcScreenRect();
                self->mPosition = LLCoordGL(screen_rect.getCenterX(), screen_rect.getCenterY()).convert();
                self->storeRectControl();
            }
            self->setMinimized(false); // to reenable minimize button if it was minimized
            new_host->showFloater(self);
            // make sure host is visible
            new_host->openFloater(new_host->getKey());
        }
        self->setTornOff(false);
    }
    self->updateTitleButtons();
    self->setOpenPositioning(LLFloaterEnums::POSITIONING_RELATIVE);
    // <FS:Ansariel> Explicitly call storeVisibilityControl() here so we don't produce
    //               stale visibility settings, especially if floaters get docked
    self->storeVisibilityControl();
}

// static
void LLFloater::onClickDock(LLFloater* self)
{
    if(self && self->mCanDock)
    {
        self->setDocked(!self->mDocked, true);
    }
}

// static
void LLFloater::onClickHelp( LLFloater* self )
{
    if (self && LLUI::getInstance()->mHelpImpl)
    {
        // find the current help context for this floater
        std::string help_topic;
        if (self->findHelpTopic(help_topic))
        {
            LLUI::getInstance()->mHelpImpl->showTopic(help_topic);
        }
    }
}

// <FS:Ansariel> FIRE-11724: Snooze group chat
void LLFloater::setCanSnooze(bool can_snooze)
{
    mCanSnooze = can_snooze;
    mButtonsEnabled[BUTTON_SNOOZE] = mCanSnooze;
    updateTitleButtons();
}

void LLFloater::onClickSnooze(LLFloater* self)
{
    if (self)
    {
        self->onSnooze();
    }
}
// </FS:Ansariel>

void LLFloater::initRectControl()
{
    // save_rect and save_visibility only apply to registered floaters
    if (mSaveRect)
    {
        std::string ctrl_name = getControlName(mInstanceName, mKey);
        mRectControl = LLFloaterReg::declareRectControl(ctrl_name);
        mPosXControl = LLFloaterReg::declarePosXControl(ctrl_name);
        mPosYControl = LLFloaterReg::declarePosYControl(ctrl_name);
    }
}

// static
void LLFloater::closeFrontmostFloater()
{
    LLFloater* floater_to_close = gFloaterView->getFrontmostClosableFloater();
    // <FS:Ansariel> CTRL-W doesn't work with multifloaters
    LLMultiFloater* multi_floater = dynamic_cast<LLMultiFloater*>(floater_to_close);
    if (multi_floater)
    {
        multi_floater->closeDockedFloater();
    }
    else
    // </FS:Ansariel>
    if(floater_to_close)
    {
        floater_to_close->closeFloater();
    }

    // if nothing took focus after closing focused floater
    // give it to next floater (to allow closing multiple windows via keyboard in rapid succession)
    if (gFocusMgr.getKeyboardFocus() == NULL)
    {
        // HACK: use gFloaterView directly in case we are using Ctrl-W to close snapshot window
        // which sits in gSnapshotFloaterView, and needs to pass focus on to normal floater view
        gFloaterView->focusFrontFloater();
    }
}


// static
void LLFloater::onClickClose( LLFloater* self )
{
    if (!self)
        return;
    self->onClickCloseBtn();
}

// static
void LLFloater::onClickClose(LLFloater* self, bool app_quitting)
{
    if (!self)
        return;
    self->onClickCloseBtn(app_quitting);
}

void LLFloater::onClickCloseBtn(bool app_quitting)
{
    // <FS:Ansariel> FIRE-24125: Add option to close all floaters of a group
    if (gKeyboard->currentMask(false) & MASK_SHIFT)
    {
        auto floaterlist = LLFloaterReg::getAllFloatersInGroup(this);
        for (auto floater : floaterlist)
        {
            if (floater != this)
            {
                floater->closeFloater();
            }
        }
    }
    // </FS:Ansariel>

    closeFloater(false);
}


// virtual
void LLFloater::draw()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    LL_PROFILE_ZONE_TEXT(getTitle().c_str(), getTitle().length());

    const F32 alpha = getCurrentTransparency();

    // draw background
    if( isBackgroundVisible() )
    {
        // <FS:Zi> Optional Drop Shadows
        if(mDropShadow)
            drawShadow(this);

        S32 left = LLPANEL_BORDER_WIDTH;
        S32 top = getRect().getHeight() - LLPANEL_BORDER_WIDTH;
        S32 right = getRect().getWidth() - LLPANEL_BORDER_WIDTH;
        S32 bottom = LLPANEL_BORDER_WIDTH;

        LLUIImage* image = NULL;
        LLColor4 color;
        LLColor4 overlay_color;
        if (isBackgroundOpaque())
        {
            // NOTE: image may not be set
            image = getBackgroundImage();
            color = getBackgroundColor();
            overlay_color = getBackgroundImageOverlay();
        }
        else
        {
            image = getTransparentImage();
            color = getTransparentColor();
            overlay_color = getTransparentImageOverlay();
        }

        if (image)
        {
            // We're using images for this floater's backgrounds
            image->draw(getLocalRect(), overlay_color % alpha);
        }
        else
        {
            // We're not using images, use old-school flat colors
            gl_rect_2d( left, top, right, bottom, color % alpha );

            // draw highlight on title bar to indicate focus.  RDW
            if(hasFocus()
                && !getIsChrome()
                && !getCurrentTitle().empty())
            {
                static LLUIColor titlebar_focus_color = LLUIColorTable::instance().getColor("TitleBarFocusColor");

                const LLFontGL* font = LLFontGL::getFontSansSerif();
                LLRect r = getRect();
                gl_rect_2d_offset_local(0, r.getHeight(), r.getWidth(), r.getHeight() - font->getLineHeight() - 1,
                    titlebar_focus_color % alpha, 0, true);
            }
        }
    }

    LLPanel::updateDefaultBtn();

    if (isMinimized())
    {
        for (S32 i = 0; i < BUTTON_COUNT; i++)
        {
            drawChild(mButtons[i]);
        }
        drawChild(mDragHandle, 0, 0, true);
    }
    else
    {
        // don't call LLPanel::draw() since we've implemented custom background rendering
        LLView::draw();
    }

    // update tearoff button for torn off floaters
    // when last host goes away
    if (mCanTearOff && !getHost())
    {
        LLFloater* old_host = mLastHostHandle.get();
        if (!old_host)
        {
            setCanTearOff(false);
        }
    }
}

void    LLFloater::drawShadow(LLPanel* panel)
{
    S32 left = LLPANEL_BORDER_WIDTH;
    S32 top = panel->getRect().getHeight() - LLPANEL_BORDER_WIDTH;
    S32 right = panel->getRect().getWidth() - LLPANEL_BORDER_WIDTH;
    S32 bottom = LLPANEL_BORDER_WIDTH;

    static LLUIColor shadow_color_cached = LLUIColorTable::instance().getColor("ColorDropShadow");
    LLColor4 shadow_color = shadow_color_cached;
    F32 shadow_offset = (F32)DROP_SHADOW_FLOATER;

    if (!panel->isBackgroundOpaque())
    {
        shadow_offset *= 0.2f;
        shadow_color.mV[VALPHA] *= 0.5f;
    }
    gl_drop_shadow(left, top, right, bottom,
        shadow_color % getCurrentTransparency(),
        ll_round(shadow_offset));
}

void LLFloater::updateTransparency(LLView* view, ETypeTransparency transparency_type)
{
    if (view)
    {
        if (view->isCtrl())
        {
            static_cast<LLUICtrl*>(view)->setTransparencyType(transparency_type);
        }

        for (LLView* pChild : *view->getChildList())
        {
            if ( (pChild->getChildCount()) || (pChild->isCtrl()) )
                updateTransparency(pChild, transparency_type);
        }
    }
}

void LLFloater::updateTransparency(ETypeTransparency transparency_type)
{
    updateTransparency(this, transparency_type);
}

void    LLFloater::setCanMinimize(bool can_minimize)
{
    // if removing minimize/restore button programmatically,
    // go ahead and unminimize floater
    mCanMinimize = can_minimize;
    if (!can_minimize)
    {
        setMinimized(false);
    }

    mButtonsEnabled[BUTTON_MINIMIZE] = can_minimize && !isMinimized();
    mButtonsEnabled[BUTTON_RESTORE]  = can_minimize &&  isMinimized();

    updateTitleButtons();
}

void    LLFloater::setCanClose(bool can_close)
{
    mCanClose = can_close;
    mButtonsEnabled[BUTTON_CLOSE] = can_close;

    updateTitleButtons();
}

void    LLFloater::setCanTearOff(bool can_tear_off)
{
    mCanTearOff = can_tear_off;
    mButtonsEnabled[BUTTON_TEAR_OFF] = mCanTearOff && !mHostHandle.isDead();

    updateTitleButtons();
}


void LLFloater::setCanResize(bool can_resize)
{
    mResizable = can_resize;
    enableResizeCtrls(can_resize);
}

void LLFloater::setCanDrag(bool can_drag)
{
    // if we delete drag handle, we no longer have access to the floater's title
    // so just enable/disable it
    if (!can_drag && mDragHandle->getEnabled())
    {
        mDragHandle->setEnabled(false);
    }
    else if (can_drag && !mDragHandle->getEnabled())
    {
        mDragHandle->setEnabled(true);
    }
}

bool LLFloater::getCanDrag() const
{
    return mDragHandle->getEnabled();
}


void LLFloater::updateTitleButtons()
{
    static LLUICachedControl<S32> floater_close_box_size ("UIFloaterCloseBoxSize", 0);
    static LLUICachedControl<S32> close_box_from_top ("UICloseBoxFromTop", 0);
    LLRect buttons_rect;
    S32 button_count = 0;
    for (S32 i = 0; i < BUTTON_COUNT; i++)
    {
        if (!mButtons[i])
        {
            continue;
        }

        bool enabled = mButtonsEnabled[i];
        if (i == BUTTON_HELP)
        {
            // don't show the help button if the floater is minimized
            // or if it is a docked tear-off floater
            if (isMinimized() || (mButtonsEnabled[BUTTON_TEAR_OFF] && ! mTornOff))
            {
                enabled = false;
            }
        }

#if 0       //KC: don't do this, see below
        if (i == BUTTON_CLOSE && mButtonScale != 1.f)
        {
            //*HACK: always render close button for hosted floaters so
            //that users don't accidentally hit the button when
            //closing multiple windows in the chatterbox
            enabled = true;
        }
#endif

        mButtons[i]->setEnabled(enabled);

        if (enabled
        //KC: don't explictly force enable close button on
        //hosted floaters instead drawing a disabled one
        || (i == BUTTON_CLOSE && mButtonScale != 1.f))
        {
            button_count++;

            LLRect btn_rect;
            if (mDragOnLeft)
            {
                btn_rect.setLeftTopAndSize(
                    LLPANEL_BORDER_WIDTH,
                    getRect().getHeight() - close_box_from_top - (floater_close_box_size + 1) * button_count,
                    ll_round((F32)floater_close_box_size * mButtonScale),
                    ll_round((F32)floater_close_box_size * mButtonScale));
            }
            else
            {
                btn_rect.setLeftTopAndSize(
                    getRect().getWidth() - LLPANEL_BORDER_WIDTH - (floater_close_box_size + 1) * button_count,
                    getRect().getHeight() - close_box_from_top,
                    ll_round((F32)floater_close_box_size * mButtonScale),
                    ll_round((F32)floater_close_box_size * mButtonScale));
            }

            // first time here, init 'buttons_rect'
            if(1 == button_count)
            {
                buttons_rect = btn_rect;
            }
            else
            {
                // if mDragOnLeft=true then buttons are on top-left side vertically aligned
                // title is not displayed in this case, calculating 'buttons_rect' for future use
                mDragOnLeft ? buttons_rect.mBottom -= btn_rect.mBottom :
                    buttons_rect.mLeft = btn_rect.mLeft;
            }
            mButtons[i]->setRect(btn_rect);
            mButtons[i]->setVisible(true);
            // the restore button should have a tab stop so that it takes action when you Ctrl-Tab to a minimized floater
            mButtons[i]->setTabStop(i == BUTTON_RESTORE);
            sendChildToFront(mButtons[i]);
        }
        else
        {
            mButtons[i]->setVisible(false);
        }
    }
    if (mDragHandle)
    {
        localRectToOtherView(buttons_rect, &buttons_rect, mDragHandle);
        mDragHandle->setButtonsRect(buttons_rect);
    }

    // <FS:Ansariel> FIRE-7782: Set correct tooltip for dock/tear-off button
    if (mButtons[BUTTON_TEAR_OFF])
    {
        if (isTornOff())
        {
            mButtons[BUTTON_TEAR_OFF]->setToolTip(sButtonToolTips[BUTTON_DOCK]);
        }
        else
        {
            mButtons[BUTTON_TEAR_OFF]->setToolTip(sButtonToolTips[BUTTON_TEAR_OFF]);
        }
    }
    // </FS:Ansariel>
}

void LLFloater::drawConeToOwner(F32 &context_cone_opacity,
                                F32 max_cone_opacity,
                                LLView *owner_view,
                                F32 fade_time,
                                F32 contex_cone_in_alpha,
                                F32 contex_cone_out_alpha)
{
    if (owner_view
        && owner_view->isInVisibleChain()
        && hasFocus()
        && context_cone_opacity > 0.001f
        && gFocusMgr.childHasKeyboardFocus(this))
    {
        // draw cone of context pointing back to owner (e.x. texture swatch)
        LLRect owner_rect;
        owner_view->localRectToOtherView(owner_view->getLocalRect(), &owner_rect, this);
        LLRect local_rect = getLocalRect();

        gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
        LLGLEnable(GL_CULL_FACE);
        gGL.begin(LLRender::TRIANGLE_STRIP);
        {
            gGL.color4f(0.f, 0.f, 0.f, contex_cone_in_alpha * context_cone_opacity);
            gGL.vertex2i(owner_rect.mLeft, owner_rect.mTop);
            gGL.color4f(0.f, 0.f, 0.f, contex_cone_out_alpha * context_cone_opacity);
            gGL.vertex2i(local_rect.mLeft, local_rect.mTop);
            gGL.color4f(0.f, 0.f, 0.f, contex_cone_in_alpha * context_cone_opacity);
            gGL.vertex2i(owner_rect.mRight, owner_rect.mTop);
            gGL.color4f(0.f, 0.f, 0.f, contex_cone_out_alpha * context_cone_opacity);
            gGL.vertex2i(local_rect.mRight, local_rect.mTop);
            gGL.color4f(0.f, 0.f, 0.f, contex_cone_in_alpha * context_cone_opacity);
            gGL.vertex2i(owner_rect.mRight, owner_rect.mBottom);
            gGL.color4f(0.f, 0.f, 0.f, contex_cone_out_alpha * context_cone_opacity);
            gGL.vertex2i(local_rect.mRight, local_rect.mBottom);
            gGL.color4f(0.f, 0.f, 0.f, contex_cone_in_alpha * context_cone_opacity);
            gGL.vertex2i(owner_rect.mLeft, owner_rect.mBottom);
            gGL.color4f(0.f, 0.f, 0.f, contex_cone_out_alpha * context_cone_opacity);
            gGL.vertex2i(local_rect.mLeft, local_rect.mBottom);
            gGL.color4f(0.f, 0.f, 0.f, contex_cone_in_alpha * context_cone_opacity);
            gGL.vertex2i(owner_rect.mLeft, owner_rect.mTop);
            gGL.color4f(0.f, 0.f, 0.f, contex_cone_out_alpha * context_cone_opacity);
            gGL.vertex2i(local_rect.mLeft, local_rect.mTop);
        }
        gGL.end();
    }

    if (gFocusMgr.childHasMouseCapture(getDragHandle()))
    {
        context_cone_opacity = lerp(context_cone_opacity, max_cone_opacity, LLSmoothInterpolation::getInterpolant(fade_time));
    }
    else
    {
        context_cone_opacity = lerp(context_cone_opacity, 0.f, LLSmoothInterpolation::getInterpolant(fade_time));
    }
}

void LLFloater::buildButtons(const Params& floater_params)
{
    static LLUICachedControl<S32> floater_close_box_size ("UIFloaterCloseBoxSize", 0);
    static LLUICachedControl<S32> close_box_from_top ("UICloseBoxFromTop", 0);
    for (S32 i = 0; i < BUTTON_COUNT; i++)
    {
        if (mButtons[i])
        {
            removeChild(mButtons[i]);
            delete mButtons[i];
            mButtons[i] = NULL;
        }

        LLRect btn_rect;
        if (mDragOnLeft)
        {
            btn_rect.setLeftTopAndSize(
                LLPANEL_BORDER_WIDTH,
                getRect().getHeight() - close_box_from_top - (floater_close_box_size + 1) * (i + 1),
                ll_round(floater_close_box_size * mButtonScale),
                ll_round(floater_close_box_size * mButtonScale));
        }
        else
        {
            btn_rect.setLeftTopAndSize(
                getRect().getWidth() - LLPANEL_BORDER_WIDTH - (floater_close_box_size + 1) * (i + 1),
                getRect().getHeight() - close_box_from_top,
                ll_round(floater_close_box_size * mButtonScale),
                ll_round(floater_close_box_size * mButtonScale));
        }

        LLButton::Params p;
        p.name(sButtonNames[i]);
        p.rect(btn_rect);
        p.image_unselected = getButtonImage(floater_params, (EFloaterButton)i);
        // Selected, no matter if hovered or not, is "pressed"
        LLUIImage* pressed_image = getButtonPressedImage(floater_params, (EFloaterButton)i);
        p.image_selected = pressed_image;
        p.image_hover_selected = pressed_image;
        // Use a glow effect when the user hovers over the button
        // These icons are really small, need glow amount increased
        p.hover_glow_amount( 0.33f );
        p.click_callback.function(boost::bind(sButtonCallbacks[i], this));
        p.tab_stop(false);
        p.follows.flags(FOLLOWS_TOP|FOLLOWS_RIGHT);
        p.tool_tip = getButtonTooltip(floater_params, (EFloaterButton)i, getIsChrome());
        p.scale_image(true);
        p.chrome(true);

        LLButton* buttonp = LLUICtrlFactory::create<LLButton>(p);
        addChild(buttonp);
        mButtons[i] = buttonp;
    }

    updateTitleButtons();
}

// static
LLUIImage* LLFloater::getButtonImage(const Params& p, EFloaterButton e)
{
    switch(e)
    {
        default:
        case BUTTON_CLOSE:
            return p.close_image;
        case BUTTON_RESTORE:
            return p.restore_image;
        case BUTTON_MINIMIZE:
            return p.minimize_image;
        case BUTTON_TEAR_OFF:
            return p.tear_off_image;
        case BUTTON_DOCK:
            return p.dock_image;
        case BUTTON_HELP:
            return p.help_image;
        // <FS:Ansariel> FIRE-11724: Snooze group chat
        case BUTTON_SNOOZE:
            return p.snooze_image;
        // </FS:Ansariel>
    }
}

// static
LLUIImage* LLFloater::getButtonPressedImage(const Params& p, EFloaterButton e)
{
    switch(e)
    {
        default:
        case BUTTON_CLOSE:
            return p.close_pressed_image;
        case BUTTON_RESTORE:
            return p.restore_pressed_image;
        case BUTTON_MINIMIZE:
            return p.minimize_pressed_image;
        case BUTTON_TEAR_OFF:
            return p.tear_off_pressed_image;
        case BUTTON_DOCK:
            return p.dock_pressed_image;
        case BUTTON_HELP:
            return p.help_pressed_image;
        // <FS:Ansariel> FIRE-11724: Snooze group chat
        case BUTTON_SNOOZE:
            return p.snooze_pressed_image;
        // </FS:Ansariel>
    }
}

// static
std::string LLFloater::getButtonTooltip(const Params& p, EFloaterButton e, bool is_chrome)
{
    // EXT-4081 (Lag Meter: Ctrl+W does not close floater)
    // If floater is chrome set 'Close' text for close button's tooltip
    if(is_chrome && BUTTON_CLOSE == e)
    {
        static std::string close_tooltip_chrome = LLTrans::getString("BUTTON_CLOSE_CHROME");
        return close_tooltip_chrome;
    }
    // TODO: per-floater localizable tooltips set in XML
    return sButtonToolTips[e];
}

/////////////////////////////////////////////////////
// LLFloaterView

static LLDefaultChildRegistry::Register<LLFloaterView> r("floater_view");

LLFloaterView::LLFloaterView (const Params& p)
:   LLUICtrl (p),
    mFocusCycleMode(false),
    mMinimizePositionVOffset(0),
    mSnapOffsetBottom(0),
    mSnapOffsetChatBar(0),
    mSnapOffsetLeft(0),
    mSnapOffsetRight(0)
{
    mSnapView = getHandle();
}

// By default, adjust vertical.
void LLFloaterView::reshape(S32 width, S32 height, bool called_from_parent)
{
    LLView::reshape(width, height, called_from_parent);

    mLastSnapRect = getSnapRect();

    for ( child_list_const_iter_t child_it = getChildList()->begin(); child_it != getChildList()->end(); ++child_it)
    {
        LLView* viewp = *child_it;
        LLFloater* floaterp = dynamic_cast<LLFloater*>(viewp);
        if (floaterp->isDependent())
        {
            // dependents are moved with their "dependee"
            continue;
        }

        if (!floaterp->isMinimized() && floaterp->getCanDrag())
        {
            LLRect old_rect = floaterp->getRect();
            floaterp->applyPositioning(NULL, false);
            LLRect new_rect = floaterp->getRect();

            //LLRect r = floaterp->getRect();

            //// Compute absolute distance from each edge of screen
            //S32 left_offset = llabs(r.mLeft - 0);
            //S32 right_offset = llabs(old_right - r.mRight);

            //S32 top_offset = llabs(old_top - r.mTop);
            //S32 bottom_offset = llabs(r.mBottom - 0);

            S32 translate_x = new_rect.mLeft - old_rect.mLeft;
            S32 translate_y = new_rect.mBottom - old_rect.mBottom;

            //if (left_offset > right_offset)
            //{
            //  translate_x = new_right - old_right;
            //}

            //if (top_offset < bottom_offset)
            //{
            //  translate_y = new_top - old_top;
            //}

            // don't reposition immovable floaters
            //if (floaterp->getCanDrag())
            //{
            //  floaterp->translate(translate_x, translate_y);
            //}
            for (LLHandle<LLFloater> dependent_floater : floaterp->mDependents)
            {
                if (dependent_floater.get())
                {
                    dependent_floater.get()->translate(translate_x, translate_y);
                }
            }
        }
    }
}


void LLFloaterView::restoreAll()
{
    // make sure all subwindows aren't minimized
    child_list_t child_list = *(getChildList());
    for (LLView* child : child_list)
    {
        LLFloater* floaterp = dynamic_cast<LLFloater*>(child);
        if (floaterp)
        {
            floaterp->setMinimized(false);
        }
    }

    // *FIX: make sure dependents are restored

    // children then deleted by default view constructor
}


LLRect LLFloaterView::findNeighboringPosition( LLFloater* reference_floater, LLFloater* neighbor )
{
    LLRect base_rect = reference_floater->getRect();
    LLRect::tCoordType width = neighbor->getRect().getWidth();
    LLRect::tCoordType height = neighbor->getRect().getHeight();
    LLRect new_rect = neighbor->getRect();

    LLRect expanded_base_rect = base_rect;
    expanded_base_rect.stretch(10);
    for(LLFloater::handle_set_iter_t dependent_it = reference_floater->mDependents.begin();
        dependent_it != reference_floater->mDependents.end(); ++dependent_it)
    {
        LLFloater* sibling = dependent_it->get();
        // check for dependents within 10 pixels of base floater
        if (sibling &&
            sibling != neighbor &&
            sibling->getVisible() &&
            expanded_base_rect.overlaps(sibling->getRect()))
        {
            base_rect.unionWith(sibling->getRect());
        }
    }

    LLRect::tCoordType left_margin = llmax(0, base_rect.mLeft);
    LLRect::tCoordType right_margin = llmax(0, getRect().getWidth() - base_rect.mRight);
    LLRect::tCoordType top_margin = llmax(0, getRect().getHeight() - base_rect.mTop);
    LLRect::tCoordType bottom_margin = llmax(0, base_rect.mBottom);

    // find position for floater in following order
    // right->left->bottom->top
    for (S32 i = 0; i < 5; i++)
    {
        if (right_margin > width)
        {
            new_rect.translate(base_rect.mRight - neighbor->getRect().mLeft, base_rect.mTop - neighbor->getRect().mTop);
            return new_rect;
        }
        else if (left_margin > width)
        {
            new_rect.translate(base_rect.mLeft - neighbor->getRect().mRight, base_rect.mTop - neighbor->getRect().mTop);
            return new_rect;
        }
        else if (bottom_margin > height)
        {
            new_rect.translate(base_rect.mLeft - neighbor->getRect().mLeft, base_rect.mBottom - neighbor->getRect().mTop);
            return new_rect;
        }
        else if (top_margin > height)
        {
            new_rect.translate(base_rect.mLeft - neighbor->getRect().mLeft, base_rect.mTop - neighbor->getRect().mBottom);
            return new_rect;
        }

        // keep growing margins to find "best" fit
        left_margin += 20;
        right_margin += 20;
        top_margin += 20;
        bottom_margin += 20;
    }

    // didn't find anything, return initial rect
    return new_rect;
}


void LLFloaterView::bringToFront(LLFloater* child, bool give_focus, bool restore)
{
    if (!child)
        return;

    LLFloater* front_child = mFrontChildHandle.get();
    if (front_child == child)
    {
        if (give_focus && child->canFocusStealFrontmost() && !gFocusMgr.childHasKeyboardFocus(child))
        {
            child->setFocus(true);
        }
        return;
    }

    if (front_child && !front_child->isDead() && front_child->getVisible())
    {
        front_child->goneFromFront();
    }

    mFrontChildHandle = child->getHandle();

    // *TODO: make this respect floater's mAutoFocus value, instead of
    // using parameter
    if (child->getHost())
    {
        // this floater is hosted elsewhere and hence not one of our children, abort
        return;
    }
    std::vector<LLFloater*> floaters_to_move;
    // Look at all floaters...tab
    for (child_list_const_iter_t child_it = beginChild(); child_it != endChild(); ++child_it)
    {
        LLFloater* floater = dynamic_cast<LLFloater*>(*child_it);

        // ...but if I'm a dependent floater...
        if (floater && child->isDependent())
        {
            // ...look for floaters that have me as a dependent...
            LLFloater::handle_set_iter_t found_dependent = floater->mDependents.find(child->getHandle());

            if (found_dependent != floater->mDependents.end())
            {
                // ...and make sure all children of that floater (including me) are brought to front...
                for (LLFloater::handle_set_iter_t dependent_it = floater->mDependents.begin();
                    dependent_it != floater->mDependents.end(); ++dependent_it)
                {
                    LLFloater* sibling = dependent_it->get();
                    if (sibling)
                    {
                        floaters_to_move.push_back(sibling);
                    }
                }
                //...before bringing my parent to the front...
                floaters_to_move.push_back(floater);
            }
        }
    }

    std::vector<LLFloater*>::iterator floater_it;
    for(floater_it = floaters_to_move.begin(); floater_it != floaters_to_move.end(); ++floater_it)
    {
        LLFloater* floaterp = *floater_it;
        sendChildToFront(floaterp);

        // always unminimize dependee, but allow dependents to stay minimized
        if (!floaterp->isDependent())
        {
            floaterp->setMinimized(false);
        }
    }
    floaters_to_move.clear();

    // ...then bringing my own dependents to the front...
    for (LLFloater::handle_set_iter_t dependent_it = child->mDependents.begin();
        dependent_it != child->mDependents.end(); ++dependent_it)
    {
        LLFloater* dependent = dependent_it->get();
        if (dependent)
        {
            sendChildToFront(dependent);
        }
    }

    // ...and finally bringing myself to front
    // (do this last, so that I'm left in front at end of this call)
    if (*beginChild() != child)
    {
        sendChildToFront(child);
    }

    if(restore)
    {
        child->setMinimized(false);
    }

    if (give_focus && !gFocusMgr.childHasKeyboardFocus(child))
    {
        child->setFocus(true);
        // floater did not take focus, so relinquish focus to world
        if (!child->hasFocus())
        {
            gFocusMgr.setKeyboardFocus(NULL);
        }
    }
}

void LLFloaterView::highlightFocusedFloater()
{
    for ( child_list_const_iter_t child_it = getChildList()->begin(); child_it != getChildList()->end(); ++child_it)
    {
        LLFloater *floater = (LLFloater *)(*child_it);

        // skip dependent floaters, as we'll handle them in a batch along with their dependee(?)
        if (floater->isDependent())
        {
            continue;
        }

        bool floater_or_dependent_has_focus = gFocusMgr.childHasKeyboardFocus(floater);
        for(LLFloater::handle_set_iter_t dependent_it = floater->mDependents.begin();
            dependent_it != floater->mDependents.end();
            ++dependent_it)
        {
            LLFloater* dependent_floaterp = dependent_it->get();
            if (dependent_floaterp && gFocusMgr.childHasKeyboardFocus(dependent_floaterp))
            {
                floater_or_dependent_has_focus = true;
            }
        }

        // now set this floater and all its dependents
        floater->setForeground(floater_or_dependent_has_focus);

        for(LLFloater::handle_set_iter_t dependent_it = floater->mDependents.begin();
            dependent_it != floater->mDependents.end(); )
        {
            LLFloater* dependent_floaterp = dependent_it->get();
            if (dependent_floaterp)
            {
                dependent_floaterp->setForeground(floater_or_dependent_has_focus);
            }
            ++dependent_it;
        }

        floater->cleanupHandles();
    }
}

LLFloater* LLFloaterView::getFrontmostClosableFloater()
{
    child_list_const_iter_t child_it;
    LLFloater* frontmost_floater = NULL;

    for ( child_it = getChildList()->begin(); child_it != getChildList()->end(); ++child_it)
    {
        frontmost_floater = (LLFloater *)(*child_it);

        if (frontmost_floater->isInVisibleChain() && frontmost_floater->isCloseable())
        {
            return frontmost_floater;
        }
    }

    return NULL;
}

void LLFloaterView::unhighlightFocusedFloater()
{
    for ( child_list_const_iter_t child_it = getChildList()->begin(); child_it != getChildList()->end(); ++child_it)
    {
        LLFloater *floater = (LLFloater *)(*child_it);

        floater->setForeground(false);
    }
}

void LLFloaterView::focusFrontFloater()
{
    LLFloater* floaterp = getFrontmost();
    if (floaterp)
    {
        floaterp->setFocus(true);
    }
}

void LLFloaterView::getMinimizePosition(S32 *left, S32 *bottom)
{
    const LLFloater::Params& default_params = LLFloater::getDefaultParams();
    S32 floater_header_size = default_params.header_height;
    static LLUICachedControl<S32> minimized_width ("UIMinimizedWidth", 0);
    LLRect snap_rect_local = getLocalSnapRect();
    snap_rect_local.mTop += mMinimizePositionVOffset;

    // <FS:KC> Minimize floaters to bottom left
    static LLUICachedControl<bool> legacy_minimize ("FSLegacyMinimize", false);
    if (legacy_minimize)
    {
        //reverse column row so floaters tile across
        S32 col = 0;
        for(S32 row = snap_rect_local.mBottom;
        row < snap_rect_local.getHeight() - floater_header_size;
        row += floater_header_size ) //loop rows
        {
            for(col = snap_rect_local.mLeft;
                col < snap_rect_local.getWidth() - minimized_width;
                col += minimized_width)
            {
                bool foundGap = true;
                for(child_list_const_iter_t child_it = getChildList()->begin();
                    child_it != getChildList()->end();
                    ++child_it) //loop floaters
                {
                    // Examine minimized children.
                    // <FS:Ansariel> Prefer dynamic_cast over c-style cast
                    //LLFloater* floater = (LLFloater*)((LLView*)*child_it);
                    LLFloater* floater = dynamic_cast<LLFloater*>(*child_it);
                    // </FS:Ansariel>
                    if(floater->isMinimized())
                    {
                        LLRect r = floater->getRect();
                        if((r.mBottom < (row + floater_header_size))
                           && (r.mBottom > (row - floater_header_size))
                           && (r.mLeft < (col + minimized_width))
                           && (r.mLeft > (col - minimized_width)))
                        {
                            // needs the check for off grid. can't drag,
                            // but window resize makes them off
                            foundGap = false;
                            break;
                        }
                    }
                } //done floaters
                if(foundGap)
                {
                    *left = col;
                    *bottom = row;
                    return; //done
                }
            } //done this col
        }
    }
    else
    {
    // </FS:KC> Minimize floaters to bottom left

    for(S32 col = snap_rect_local.mLeft;
        col < snap_rect_local.getWidth() - minimized_width;
        col += minimized_width)
    {
        // <FS:AO> offset minimized windows to not obscure title bars. Yes, this is a quick and dirty hack.
        int offset = 0;
        //LLFavoritesBarCtrl* fb = getChild<LLFavoritesBarCtrl>("favorite");
        bool fbVisible = LLUI::getInstance()->mSettingGroups["config"]->getBOOL("ShowNavbarFavoritesPanel");
        bool nbVisible = LLUI::getInstance()->mSettingGroups["config"]->getBOOL("ShowNavbarNavigationPanel");
        // TODO: Make this introspect controls to get the dynamic size.
        if (fbVisible)
            offset += 20;
        if (nbVisible)
            offset += 30;
        // </FS:AO>

        for(S32 row = snap_rect_local.mTop - (floater_header_size + offset);
        row > floater_header_size;
        row -= floater_header_size ) //loop rows
        {

            bool foundGap = true;
            for(child_list_const_iter_t child_it = getChildList()->begin();
                child_it != getChildList()->end();
                ++child_it) //loop floaters
            {
                // Examine minimized children.
                LLFloater* floater = dynamic_cast<LLFloater*>(*child_it);
                if(floater->isMinimized())
                {
                    LLRect r = floater->getRect();
                    if((r.mBottom < (row + floater_header_size))
                       && (r.mBottom > (row - floater_header_size))
                       && (r.mLeft < (col + minimized_width))
                       && (r.mLeft > (col - minimized_width)))
                    {
                        // needs the check for off grid. can't drag,
                        // but window resize makes them off
                        foundGap = false;
                        break;
                    }
                }
            } //done floaters
            if(foundGap)
            {
                *left = col;
                *bottom = row;
                return; //done
            }
        } //done this col
    }

    } // <FS:KC> Minimize floaters to bottom left

    // crude - stack'em all at 0,0 when screen is full of minimized
    // floaters.
    *left = snap_rect_local.mLeft;
    *bottom = snap_rect_local.mBottom;
}


void LLFloaterView::destroyAllChildren()
{
    LLView::deleteAllChildren();
}

void LLFloaterView::closeAllChildren(bool app_quitting)
{
    // iterate over a copy of the list, because closing windows will destroy
    // some windows on the list.
    child_list_t child_list = *(getChildList());

    for (child_list_const_iter_t it = child_list.begin(); it != child_list.end(); ++it)
    {
        LLView* viewp = *it;
        child_list_const_iter_t exists = std::find(getChildList()->begin(), getChildList()->end(), viewp);
        if (exists == getChildList()->end())
        {
            // this floater has already been removed
            continue;
        }

        LLFloater* floaterp = dynamic_cast<LLFloater*>(viewp);

        // Attempt to close floater.  This will cause the "do you want to save"
        // dialogs to appear.
        // Skip invisible floaters if we're not quitting (STORM-192).
        if (floaterp->canClose() && !floaterp->isDead() &&
            (app_quitting || floaterp->getVisible()))
        {
            floaterp->closeFloater(app_quitting);
        }
    }
}

void LLFloaterView::hiddenFloaterClosed(LLFloater* floater)
{
    for (hidden_floaters_t::iterator it = mHiddenFloaters.begin(), end_it = mHiddenFloaters.end();
        it != end_it;
        ++it)
    {
        if (it->first.get() == floater)
        {
            it->second.disconnect();
            mHiddenFloaters.erase(it);
            break;
        }
    }
}

void LLFloaterView::hideAllFloaters()
{
    child_list_t child_list = *(getChildList());

    for (child_list_iter_t it = child_list.begin(); it != child_list.end(); ++it)
    {
        LLFloater* floaterp = dynamic_cast<LLFloater*>(*it);
        if (floaterp && floaterp->getVisible())
        {
            floaterp->setVisible(false);
            boost::signals2::connection connection = floaterp->mCloseSignal.connect(boost::bind(&LLFloaterView::hiddenFloaterClosed, this, floaterp));
            mHiddenFloaters.push_back(std::make_pair(floaterp->getHandle(), connection));
        }
    }
}

void LLFloaterView::showHiddenFloaters()
{
    for (hidden_floaters_t::iterator it = mHiddenFloaters.begin(), end_it = mHiddenFloaters.end();
        it != end_it;
        ++it)
    {
        LLFloater* floaterp = it->first.get();
        if (floaterp)
        {
            floaterp->setVisible(true);
        }
        it->second.disconnect();
    }
    mHiddenFloaters.clear();
}

bool LLFloaterView::allChildrenClosed()
{
    // see if there are any visible floaters (some floaters "close"
    // by setting themselves invisible)
    for (child_list_const_iter_t it = getChildList()->begin(); it != getChildList()->end(); ++it)
    {
        LLFloater* floaterp = dynamic_cast<LLFloater*>(*it);

        if (floaterp->getVisible() && !floaterp->isDead() && floaterp->isCloseable())
        {
            return false;
        }
    }
    return true;
}

void LLFloaterView::shiftFloaters(S32 x_offset, S32 y_offset)
{
    for (child_list_const_iter_t it = getChildList()->begin(); it != getChildList()->end(); ++it)
    {
        LLFloater* floaterp = dynamic_cast<LLFloater*>(*it);

        if (floaterp && floaterp->isMinimized())
        {
            floaterp->translate(x_offset, y_offset);
        }
    }
}

void LLFloaterView::refresh()
{
    // <FS:KC> Fix for bad edge snapping
    // This will stop jumping floaters on chatbar show
    // But will stop floaters from bumping out of the way of toolbars
    static LLUICachedControl<bool> legacy_snap ("FSLegacyEdgeSnap", false);
    if (!legacy_snap)
    {
        LLRect snap_rect = getSnapRect();
        if (snap_rect != mLastSnapRect)
        {
            reshape(getRect().getWidth(), getRect().getHeight(), true);
        }
    }// <FS:KC> Fix for bad edge snapping

    // Constrain children to be entirely on the screen
    for ( child_list_const_iter_t child_it = getChildList()->begin(); child_it != getChildList()->end(); ++child_it)
    {
        LLFloater* floaterp = dynamic_cast<LLFloater*>(*child_it);
        if (floaterp && floaterp->getVisible() )
        {
            // minimized floaters are kept fully onscreen
            adjustToFitScreen(floaterp, !floaterp->isMinimized());
        }
    }
}

void LLFloaterView::adjustToFitScreen(LLFloater* floater, bool allow_partial_outside, bool snap_in_toolbars/* = false*/)
{
    if (floater->getParent() != this)
    {
        // floater is hosted elsewhere, so ignore
        return;
    }

    if (floater->getDependee() &&
        floater->getDependee() == floater->getSnapTarget().get())
    {
        // floater depends on other and snaps to it, so ignore
        return;
    }

    LLRect::tCoordType screen_width = getSnapRect().getWidth();
    LLRect::tCoordType screen_height = getSnapRect().getHeight();

    // only automatically resize non-minimized, resizable floaters
    if( floater->isResizable() && !floater->isMinimized() )
    {
        LLRect view_rect = floater->getRect();
        S32 old_width = view_rect.getWidth();
        S32 old_height = view_rect.getHeight();
        S32 min_width;
        S32 min_height;
        floater->getResizeLimits( &min_width, &min_height );

        // Make sure floater isn't already smaller than its min height/width?
        S32 new_width = llmax( min_width, old_width );
        S32 new_height = llmax( min_height, old_height);

        if((new_width > screen_width) || (new_height > screen_height))
        {
            // We have to make this window able to fit on screen
            new_width = llmin(new_width, screen_width);
            new_height = llmin(new_height, screen_height);

            // ...while respecting minimum width/height
            new_width = llmax(new_width, min_width);
            new_height = llmax(new_height, min_height);

            LLRect new_rect;
            new_rect.setLeftTopAndSize(view_rect.mLeft,view_rect.mTop,new_width, new_height);

            floater->setShape(new_rect);

            if (floater->followsRight())
            {
                floater->translate(old_width - new_width, 0);
            }

            if (floater->followsTop())
            {
                floater->translate(0, old_height - new_height);
            }
        }
    }

    const LLRect& constraint = snap_in_toolbars ? getSnapRect() : gFloaterView->getRect();
    S32 min_overlap_pixels = allow_partial_outside ? FLOATER_MIN_VISIBLE_PIXELS : S32_MAX;

    // <FS:Ansariel> Fix floater relocation
    //floater->fitWithDependentsOnScreen(mToolbarLeftRect, mToolbarBottomRect, mToolbarRightRect, constraint, min_overlap_pixels);
    floater->fitWithDependentsOnScreen(mToolbarLeftRect, mToolbarBottomRect, mToolbarRightRect, mMainChatbarRect, mUtilityBarRect, constraint, min_overlap_pixels);
}

void LLFloaterView::draw()
{
    refresh();

    // hide focused floater if in cycle mode, so that it can be drawn on top
    LLFloater* focused_floater = getFocusedFloater();

    if (mFocusCycleMode && focused_floater)
    {
        child_list_const_iter_t child_it = getChildList()->begin();
        for (;child_it != getChildList()->end(); ++child_it)
        {
            if ((*child_it) != focused_floater)
            {
                drawChild(*child_it);
            }
        }

        drawChild(focused_floater, -TABBED_FLOATER_OFFSET, TABBED_FLOATER_OFFSET);
    }
    else
    {
        LLView::draw();
    }
}

LLRect LLFloaterView::getSnapRect() const
{
    LLRect snap_rect = getLocalRect();

    // <FS:KC> Fix for bad edge snapping
    static LLUICachedControl<bool> legacy_snap ("FSLegacyEdgeSnap", false);
    if (legacy_snap)
    {
        snap_rect.mBottom += (mSnapOffsetBottom + mSnapOffsetChatBar);
        snap_rect.mLeft += mSnapOffsetLeft;
        snap_rect.mRight -= mSnapOffsetRight;
    }
    else
    {
    // <\FS:KC> Fix for bad edge snapping
    LLView* snap_view = mSnapView.get();
    if (snap_view)
    {
        snap_view->localRectToOtherView(snap_view->getLocalRect(), &snap_rect, this);
    }
    }// <FS:KC> Fix for bad edge snapping

    return snap_rect;
}

LLFloater *LLFloaterView::getFocusedFloater() const
{
    for ( child_list_const_iter_t child_it = getChildList()->begin(); child_it != getChildList()->end(); ++child_it)
    {
        if ((*child_it)->isCtrl())
        {
            LLFloater* ctrlp = dynamic_cast<LLFloater*>(*child_it);
            if ( ctrlp && ctrlp->hasFocus() )
            {
                return ctrlp;
            }
        }
    }
    return NULL;
}

LLFloater *LLFloaterView::getFrontmost() const
{
    for ( child_list_const_iter_t child_it = getChildList()->begin(); child_it != getChildList()->end(); ++child_it)
    {
        LLView* viewp = *child_it;
        if ( viewp->getVisible() && !viewp->isDead())
        {
            return (LLFloater *)viewp;
        }
    }
    return NULL;
}

LLFloater *LLFloaterView::getBackmost() const
{
    LLFloater* back_most = NULL;
    for ( child_list_const_iter_t child_it = getChildList()->begin(); child_it != getChildList()->end(); ++child_it)
    {
        LLView* viewp = *child_it;
        if ( viewp->getVisible() )
        {
            back_most = (LLFloater *)viewp;
        }
    }
    return back_most;
}

void LLFloaterView::syncFloaterTabOrder()
{
    LLFloater* front_child = mFrontChildHandle.get();
    if (front_child && front_child->getIsChrome())
        return;

    // look for a visible modal dialog, starting from first
    LLModalDialog* modal_dialog = NULL;
    for ( child_list_const_iter_t child_it = getChildList()->begin(); child_it != getChildList()->end(); ++child_it)
    {
        LLModalDialog* dialog = dynamic_cast<LLModalDialog*>(*child_it);
        if (dialog && dialog->isModal() && dialog->getVisible())
        {
            modal_dialog = dialog;
            break;
        }
    }

    if (modal_dialog)
    {
        // If we have a visible modal dialog, make sure that it has focus
        LLUI::getInstance()->addPopup(modal_dialog);

        if( !gFocusMgr.childHasKeyboardFocus( modal_dialog ) )
        {
            modal_dialog->setFocus(true);
        }

        if( !gFocusMgr.childHasMouseCapture( modal_dialog ) )
        {
            gFocusMgr.setMouseCapture( modal_dialog );
        }
    }
    else
    {
        // otherwise, make sure the focused floater is in the front of the child list
        for ( child_list_const_reverse_iter_t child_it = getChildList()->rbegin(); child_it != getChildList()->rend(); ++child_it)
        {
            LLFloater* floaterp = dynamic_cast<LLFloater*>(*child_it);
            if (gFocusMgr.childHasKeyboardFocus(floaterp))
            {
                LLFloater* front_child = mFrontChildHandle.get();
                if (front_child != floaterp)
                {
                    // Grab a list of the top floaters that want to stay on top of the focused floater
                    std::list<LLFloater*> listTop;
                    if (front_child && !front_child->canFocusStealFrontmost())
                    {
                        for (LLView* childp : *getChildList())
                        {
                            LLFloater* child_floaterp = static_cast<LLFloater*>(childp);
                            if (child_floaterp->canFocusStealFrontmost())
                                break;
                            listTop.push_back(child_floaterp);
                        }
                    }

                    bringToFront(floaterp, false);

                    // Restore top floaters
                    if (!listTop.empty())
                    {
                        for (LLView* childp : listTop)
                        {
                            sendChildToFront(childp);
                        }
                        mFrontChildHandle = listTop.back()->getHandle();
                    }
                }

                break;
            }
        }
    }
}

LLFloater*  LLFloaterView::getParentFloater(LLView* viewp) const
{
    LLView* parentp = viewp->getParent();

    while(parentp && parentp != this)
    {
        viewp = parentp;
        parentp = parentp->getParent();
    }

    if (parentp == this)
    {
        return dynamic_cast<LLFloater*>(viewp);
    }

    return NULL;
}

S32 LLFloaterView::getZOrder(LLFloater* child)
{
    S32 rv = 0;
    for ( child_list_const_iter_t child_it = getChildList()->begin(); child_it != getChildList()->end(); ++child_it)
    {
        LLView* viewp = *child_it;
        if(viewp == child)
        {
            break;
        }
        ++rv;
    }
    return rv;
}

void LLFloaterView::pushVisibleAll(bool visible, const skip_list_t& skip_list)
{
    for (child_list_const_iter_t child_iter = getChildList()->begin();
         child_iter != getChildList()->end(); ++child_iter)
    {
        LLView *view = *child_iter;
        if (skip_list.find(view) == skip_list.end())
        {
            view->pushVisible(visible);
        }
    }

    LLFloaterReg::blockShowFloaters(true);
}

void LLFloaterView::popVisibleAll(const skip_list_t& skip_list)
{
    // make a copy of the list since some floaters change their
    // order in the childList when changing visibility.
    child_list_t child_list_copy = *getChildList();

    for (child_list_const_iter_t child_iter = child_list_copy.begin();
         child_iter != child_list_copy.end(); ++child_iter)
    {
        LLView *view = *child_iter;
        if (skip_list.find(view) == skip_list.end())
        {
            view->popVisible();
        }
    }

    LLFloaterReg::blockShowFloaters(false);
}

void LLFloaterView::setToolbarRect(LLToolBarEnums::EToolBarLocation tb, const LLRect& toolbar_rect)
{
    switch (tb)
    {
    case LLToolBarEnums::TOOLBAR_LEFT:
        mToolbarLeftRect = toolbar_rect;
        break;
    case LLToolBarEnums::TOOLBAR_BOTTOM:
        mToolbarBottomRect = toolbar_rect;
        break;
    case LLToolBarEnums::TOOLBAR_RIGHT:
        mToolbarRightRect = toolbar_rect;
        break;
    default:
        LL_WARNS() << "setToolbarRect() passed odd toolbar number " << (S32) tb << LL_ENDL;
        break;
    }
}

// <FS:Ansariel> Prevent floaters being dragged under main chat bar
void LLFloaterView::setMainChatbarRect(LLLayoutPanel* panel, const LLRect& chatbar_rect)
{
    panel->localRectToScreen(chatbar_rect, &mMainChatbarRect);
    mMainChatbarRect.stretch(FLOATER_MIN_VISIBLE_PIXELS);
}

void LLFloaterView::setUtilityBarRect(LLLayoutPanel* panel, const LLRect& utility_bar_rect)
{
    panel->localRectToScreen(utility_bar_rect, &mUtilityBarRect);
    mUtilityBarRect.mLeft = mUtilityBarRect.mRight;
    // Just assume right end of utility bar is always the border of the window
    mUtilityBarRect.mRight = S32_MAX;
}

const LLRect& LLFloaterView::getToolbarRect(LLToolBarEnums::EToolBarLocation tb) const
{
    switch (tb)
    {
    case LLToolBarEnums::TOOLBAR_LEFT:
        return mToolbarLeftRect;
    case LLToolBarEnums::TOOLBAR_BOTTOM:
        return mToolbarBottomRect;
    case LLToolBarEnums::TOOLBAR_RIGHT:
        return mToolbarRightRect;
    default:
        LL_WARNS() << "getToolbarRect() passed odd toolbar number " << (S32) tb << LL_ENDL;
        return LLRect::null;
    }
}
// </FS:Ansariel>

void LLFloater::setInstanceName(const std::string& name)
{
    if (name != mInstanceName)
    {
    llassert_always(mInstanceName.empty());
    mInstanceName = name;
    if (!mInstanceName.empty())
    {
        std::string ctrl_name = getControlName(mInstanceName, mKey);
            initRectControl();
        if (!mVisibilityControl.empty())
        {
            mVisibilityControl = LLFloaterReg::declareVisibilityControl(ctrl_name);
        }
        if(!mDocStateControl.empty())
        {
            mDocStateControl = LLFloaterReg::declareDockStateControl(ctrl_name);
        }
    }
}
}

void LLFloater::setKey(const LLSD& newkey)
{
    // Note: We don't have to do anything special with registration when we change keys
    mKey = newkey;
}

//static
void LLFloater::setupParamsForExport(Params& p, LLView* parent)
{
    // Do rectangle munging to topleft layout first
    LLPanel::setupParamsForExport(p, parent);

    // Copy the rectangle out to apply layout constraints
    LLRect rect = p.rect;

    // Null out other settings
    p.rect.left.setProvided(false);
    p.rect.top.setProvided(false);
    p.rect.right.setProvided(false);
    p.rect.bottom.setProvided(false);

    // Explicitly set width/height
    p.rect.width.set( rect.getWidth(), true );
    p.rect.height.set( rect.getHeight(), true );

    // If you can't resize this floater, don't export min_height
    // and min_width
    bool can_resize = p.can_resize;
    if (!can_resize)
    {
        p.min_height.setProvided(false);
        p.min_width.setProvided(false);
    }
}

void LLFloater::initFromParams(const LLFloater::Params& p)
{
    // *NOTE: We have too many classes derived from LLFloater to retrofit them
    // all to pass in params via constructors.  So we use this method.

     // control_name, tab_stop, focus_lost_callback, initial_value, rect, enabled, visible
    LLPanel::initFromParams(p);

    // override any follows flags
    if (mPositioning != LLFloaterEnums::POSITIONING_SPECIFIED)
    {
        setFollows(FOLLOWS_NONE);
    }

    mTitle = p.title;
    mShortTitle = p.short_title;
    applyTitle();

    setCanTearOff(p.can_tear_off);
    setCanMinimize(p.can_minimize);
    setCanClose(p.can_close);
    setCanDock(p.can_dock);
    setCanResize(p.can_resize);
    setResizeLimits(p.min_width, p.min_height);

    mDragOnLeft = p.can_drag_on_left;
    mHeaderHeight = p.header_height;
    mLegacyHeaderHeight = p.legacy_header_height;
    mSingleInstance = p.single_instance;
    mReuseInstance = p.reuse_instance.isProvided() ? p.reuse_instance : p.single_instance;

    mDefaultRelativeX = p.rel_x;
    mDefaultRelativeY = p.rel_y;

    mPositioning = p.positioning;
    mAutoClose = p.auto_close;

    mHostedFloaterShowtitlebar = p.hosted_floater_show_titlebar; // <FS:Ansariel> MultiFloater without titlebar for hosted floater

    // <FS:Zi> Optional Drop Shadows
    // we do this here because the values in the constructor get ignored, probably due to
    // the comment at the beginning of this method. -Zi
    mDropShadow = p.drop_shadow;

    mSaveRect = p.save_rect;
    if (p.save_visibility)
    {
        mVisibilityControl = "t"; // flag to build mVisibilityControl name once mInstanceName is set
    }
    if(p.save_dock_state)
    {
        mDocStateControl = "t"; // flag to build mDocStateControl name once mInstanceName is set
    }

    // open callback
    if (p.open_callback.isProvided())
    {
        setOpenCallback(initCommitCallback(p.open_callback));
    }
    // close callback
    if (p.close_callback.isProvided())
    {
        setCloseCallback(initCommitCallback(p.close_callback));
    }

    if (mDragHandle)
    {
        mDragHandle->setTitleVisible(p.show_title);
    }
}

boost::signals2::connection LLFloater::setMinimizeCallback( const commit_signal_t::slot_type& cb )
{
    if (!mMinimizeSignal) mMinimizeSignal = new commit_signal_t();
    return mMinimizeSignal->connect(cb);
}

boost::signals2::connection LLFloater::setOpenCallback( const commit_signal_t::slot_type& cb )
{
    return mOpenSignal.connect(cb);
}

boost::signals2::connection LLFloater::setCloseCallback( const commit_signal_t::slot_type& cb )
{
    return mCloseSignal.connect(cb);
}

bool LLFloater::initFloaterXML(LLXMLNodePtr node, LLView *parent, const std::string& filename, LLXMLNodePtr output_node)
{
    LL_PROFILE_ZONE_SCOPED;
    Params default_params(LLUICtrlFactory::getDefaultParams<LLFloater>());
    Params params(default_params);

    LLXUIParser parser;
    parser.readXUI(node, params, filename); // *TODO: Error checking

    std::string xml_filename = params.filename;

    if (!xml_filename.empty())
    {
        LLXMLNodePtr referenced_xml;

        if (output_node)
        {
            //if we are exporting, we want to export the current xml
            //not the referenced xml
            Params output_params;
            parser.readXUI(node, output_params, LLUICtrlFactory::getInstance()->getCurFileName());
            setupParamsForExport(output_params, parent);
            output_node->setName(node->getName()->mString);
            parser.writeXUI(output_node, output_params, LLInitParam::default_parse_rules(), &default_params);
            return true;
        }

        LLUICtrlFactory::instance().pushFileName(xml_filename);

        if (!LLUICtrlFactory::getLayeredXMLNode(xml_filename, referenced_xml))
        {
            LL_WARNS() << "Couldn't parse panel from: " << xml_filename << LL_ENDL;

            return false;
        }

        Params referenced_params;
        parser.readXUI(referenced_xml, referenced_params, LLUICtrlFactory::getInstance()->getCurFileName());
        params.fillFrom(referenced_params);

        // add children using dimensions from referenced xml for consistent layout
        setShape(params.rect);
        LLUICtrlFactory::createChildren(this, referenced_xml, child_registry_t::instance());

        LLUICtrlFactory::instance().popFileName();
    }


    if (output_node)
    {
        Params output_params(params);
        setupParamsForExport(output_params, parent);
        output_node->setName(node->getName()->mString);
        parser.writeXUI(output_node, output_params, LLInitParam::default_parse_rules(), &default_params);
    }

    // Default floater position to top-left corner of screen
    // However, some legacy floaters have explicit top or bottom
    // coordinates set, so respect their wishes.
    if (!params.rect.top.isProvided() && !params.rect.bottom.isProvided())
    {
        params.rect.top.set(0);
    }
    if (!params.rect.left.isProvided() && !params.rect.right.isProvided())
    {
        params.rect.left.set(0);
    }
    params.from_xui = true;
    applyXUILayout(params, parent, parent == gFloaterView ? gFloaterView->getSnapRect() : parent->getLocalRect());
    initFromParams(params);

    initFloater(params);

    LLMultiFloater* last_host = LLFloater::getFloaterHost();
    if (node->hasName("multi_floater"))
    {
        LLFloater::setFloaterHost((LLMultiFloater*) this);
    }

    LLUICtrlFactory::createChildren(this, node, child_registry_t::instance(), output_node);

    if (node->hasName("multi_floater"))
    {
        LLFloater::setFloaterHost(last_host);
    }

    // HACK: When we changed the header height to 25 pixels in Viewer 2, rather
    // than re-layout all the floaters we use this value in pixels to make the
    // whole floater bigger and change the top-left coordinate for widgets.
    // The goal is to eventually set mLegacyHeaderHeight to zero, which would
    // make the top-left corner for widget layout the same as the top-left
    // corner of the window's content area.  James
    S32 header_stretch = (mHeaderHeight - mLegacyHeaderHeight);
    if (header_stretch > 0)
    {
        // Stretch the floater vertically, don't move widgets
        LLRect rect = getRect();
        rect.mTop += header_stretch;

        // This will also update drag handle, title bar, close box, etc.
        setRect(rect);
    }

    bool result;
    result = postBuild();

    if (!result)
    {
        LL_ERRS() << "Failed to construct floater " << getName() << LL_ENDL;
    }

    applyRectControl(); // If we have a saved rect control, apply it
    gFloaterView->adjustToFitScreen(this, false); // Floaters loaded from XML should all fit on screen

    moveResizeHandlesToFront();

    applyDockState();

    return true; // *TODO: Error checking
}

bool LLFloater::isShown() const
{
    return ! isMinimized() && isInVisibleChain();
}

bool LLFloater::isDetachedAndNotMinimized()
{
    return !getHost() && !isMinimized();
}

/* static */
bool LLFloater::isShown(const LLFloater* floater)
{
    return floater && floater->isShown();
}

/* static */
bool LLFloater::isMinimized(const LLFloater* floater)
{
    return floater && floater->isMinimized();
}

/* static */
bool LLFloater::isVisible(const LLFloater* floater)
{
    return floater && floater->getVisible();
}

bool LLFloater::buildFromFile(const std::string& filename)
{
    LL_PROFILE_ZONE_SCOPED;
    LLXMLNodePtr root;

    if (!LLUICtrlFactory::getLayeredXMLNode(filename, root))
    {
        LL_WARNS() << "Couldn't find (or parse) floater from: " << filename << LL_ENDL;
        return false;
    }

    // root must be called floater
    if( !(root->hasName("floater") || root->hasName("multi_floater")) )
    {
        LL_WARNS() << "Root node should be named floater in: " << filename << LL_ENDL;
        return false;
    }

    bool res = true;

    LL_DEBUGS() << "Building floater " << filename << LL_ENDL;
    LLUICtrlFactory::instance().pushFileName(filename);
    {
        if (!getFactoryMap().empty())
        {
            LLPanel::sFactoryStack.push_front(&getFactoryMap());
        }

         // for local registry callbacks; define in constructor, referenced in XUI or postBuild
        getCommitCallbackRegistrar().pushScope();
        getEnableCallbackRegistrar().pushScope();

        res = initFloaterXML(root, getParent(), filename, NULL);

        setXMLFilename(filename);

        getCommitCallbackRegistrar().popScope();
        getEnableCallbackRegistrar().popScope();

        if (!getFactoryMap().empty())
        {
            LLPanel::sFactoryStack.pop_front();
        }
    }
    LLUICtrlFactory::instance().popFileName();

    return res;
}

void LLFloater::stackWith(LLFloater& other)
{
    static LLUICachedControl<S32> floater_offset ("UIFloaterOffset", 16);

    LLRect next_rect;
    if (other.getHost())
    {
        next_rect = other.getHost()->getRect();
    }
    else
    {
        next_rect = other.getRect();
    }
    next_rect.translate(floater_offset, -floater_offset);

    const LLRect& rect = getControlGroup()->getRect(mRectControl);
    if (rect.notEmpty() && !mDefaultRectForGroup && mResizable)
    {
        next_rect.setLeftTopAndSize(next_rect.mLeft, next_rect.mTop, llmax(mMinWidth, rect.getWidth()), llmax(mMinHeight, rect.getHeight()));
    }
    else
    {
        next_rect.setLeftTopAndSize(next_rect.mLeft, next_rect.mTop, getRect().getWidth(), getRect().getHeight());
    }
    setShape(next_rect);

    if (!other.getHost())
    {
        other.mPositioning = LLFloaterEnums::POSITIONING_CASCADE_GROUP;
        other.setFollows(FOLLOWS_LEFT | FOLLOWS_TOP);
    }
}

void LLFloater::applyRelativePosition()
{
    LLRect snap_rect = gFloaterView->getSnapRect();
    LLRect floater_view_screen_rect = gFloaterView->calcScreenRect();
    snap_rect.translate(floater_view_screen_rect.mLeft, floater_view_screen_rect.mBottom);
    LLRect floater_screen_rect = calcScreenRect();

    LLCoordGL new_center = mPosition.convert();
    LLCoordGL cur_center(floater_screen_rect.getCenterX(), floater_screen_rect.getCenterY());
    translate(new_center.mX - cur_center.mX, new_center.mY - cur_center.mY);
}


LLCoordFloater::LLCoordFloater(F32 x, F32 y, LLFloater& floater)
:   coord_t(x, y)
{
    mFloater = floater.getHandle();
}


LLCoordFloater::LLCoordFloater(const LLCoordCommon& other, LLFloater& floater)
{
    mFloater = floater.getHandle();
    convertFromCommon(other);
}

LLCoordFloater& LLCoordFloater::operator=(const LLCoordFloater& other)
{
    mFloater = other.mFloater;
    coord_t::operator =(other);
    return *this;
}

void LLCoordFloater::setFloater(LLFloater& floater)
{
    mFloater = floater.getHandle();
}

bool LLCoordFloater::operator==(const LLCoordFloater& other) const
{
    return mX == other.mX && mY == other.mY && mFloater == other.mFloater;
}

LLCoordCommon LL_COORD_FLOATER::convertToCommon() const
{
    const LLCoordFloater& self = static_cast<const LLCoordFloater&>(LLCoordFloater::getTypedCoords(*this));

    LLRect snap_rect = gFloaterView->getSnapRect();
    LLRect floater_view_screen_rect = gFloaterView->calcScreenRect();
    snap_rect.translate(floater_view_screen_rect.mLeft, floater_view_screen_rect.mBottom);

    LLFloater* floaterp = mFloater.get();
    S32 floater_width = floaterp ? floaterp->getRect().getWidth() : 0;
    S32 floater_height = floaterp ? floaterp->getRect().getHeight() : 0;
    LLCoordCommon out;
    if (self.mX < -0.5f)
    {
        out.mX = ll_round(rescale(self.mX, -1.f, -0.5f, (F32)(snap_rect.mLeft - (floater_width - FLOATER_MIN_VISIBLE_PIXELS)), (F32)snap_rect.mLeft));
    }
    else if (self.mX > 0.5f)
    {
        out.mX = ll_round(rescale(self.mX, 0.5f, 1.f, (F32)(snap_rect.mRight - floater_width), (F32)(snap_rect.mRight - FLOATER_MIN_VISIBLE_PIXELS)));
    }
    else
    {
        out.mX = ll_round(rescale(self.mX, -0.5f, 0.5f, (F32)snap_rect.mLeft, (F32)(snap_rect.mRight - floater_width)));
    }

    if (self.mY < -0.5f)
    {
        out.mY = ll_round(rescale(self.mY, -1.f, -0.5f, (F32)(snap_rect.mBottom - (floater_height - FLOATER_MIN_VISIBLE_PIXELS)), (F32)snap_rect.mBottom));
    }
    else if (self.mY > 0.5f)
    {
        out.mY = ll_round(rescale(self.mY, 0.5f, 1.f, (F32)(snap_rect.mTop - floater_height), (F32)(snap_rect.mTop - FLOATER_MIN_VISIBLE_PIXELS)));
    }
    else
    {
        out.mY = ll_round(rescale(self.mY, -0.5f, 0.5f, (F32)snap_rect.mBottom, (F32)(snap_rect.mTop - floater_height)));
    }

    // return center point instead of lower left
    out.mX += floater_width / 2;
    out.mY += floater_height / 2;

    return out;
}

void LL_COORD_FLOATER::convertFromCommon(const LLCoordCommon& from)
{
    LLCoordFloater& self = static_cast<LLCoordFloater&>(LLCoordFloater::getTypedCoords(*this));
    LLRect snap_rect = gFloaterView->getSnapRect();
    LLRect floater_view_screen_rect = gFloaterView->calcScreenRect();
    snap_rect.translate(floater_view_screen_rect.mLeft, floater_view_screen_rect.mBottom);


    LLFloater* floaterp = mFloater.get();
    S32 floater_width = floaterp ? floaterp->getRect().getWidth() : 0;
    S32 floater_height = floaterp ? floaterp->getRect().getHeight() : 0;

    S32 from_x = from.mX - floater_width / 2;
    S32 from_y = from.mY - floater_height / 2;

    if (from_x < snap_rect.mLeft)
    {
        self.mX = rescale((F32)from_x, (F32)(snap_rect.mLeft - (floater_width - FLOATER_MIN_VISIBLE_PIXELS)), (F32)snap_rect.mLeft, -1.f, -0.5f);
    }
    else if (from_x + floater_width > snap_rect.mRight)
    {
        self.mX = rescale((F32)from_x, (F32)(snap_rect.mRight - floater_width), (F32)(snap_rect.mRight - FLOATER_MIN_VISIBLE_PIXELS), 0.5f, 1.f);
    }
    else
    {
        self.mX = rescale((F32)from_x, (F32)snap_rect.mLeft, (F32)(snap_rect.mRight - floater_width), -0.5f, 0.5f);
    }

    if (from_y < snap_rect.mBottom)
    {
        self.mY = rescale((F32)from_y, (F32)(snap_rect.mBottom - (floater_height - FLOATER_MIN_VISIBLE_PIXELS)), (F32)snap_rect.mBottom, -1.f, -0.5f);
    }
    else if (from_y + floater_height > snap_rect.mTop)
    {
        self.mY = rescale((F32)from_y, (F32)(snap_rect.mTop - floater_height), (F32)(snap_rect.mTop - FLOATER_MIN_VISIBLE_PIXELS), 0.5f, 1.f);
    }
    else
    {
        self.mY = rescale((F32)from_y, (F32)snap_rect.mBottom, (F32)(snap_rect.mTop - floater_height), -0.5f, 0.5f);
    }
}
