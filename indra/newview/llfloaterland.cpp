/**
 * @file llfloaterland.cpp
 * @brief "About Land" floater, allowing display and editing of land parcel properties.
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

#include "llviewerprecompiledheaders.h"

#include <sstream>
#include <time.h>

#include "llfloaterland.h"

#include "llavatarnamecache.h"
#include "llfocusmgr.h"
#include "llnotificationsutil.h"
#include "llparcel.h"
#include "message.h"

#include "llagent.h"
#include "llagentaccess.h"
#include "llappviewer.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llfloaterreg.h"
#include "llfloateravatarpicker.h"
#include "llfloaterauction.h"
#include "llfloaterbanduration.h"
#include "llfloatergroups.h"
#include "llfloaterscriptlimits.h"
#include "llavataractions.h"
#include "lllineeditor.h"
#include "llnamelistctrl.h"
#include "llpanellandaudio.h"
#include "llpanellandmedia.h"
#include "llradiogroup.h"
#include "llresmgr.h"                   // getMonetaryString
#include "llscrolllistctrl.h"
#include "llscrolllistitem.h"
#include "llscrolllistcell.h"
#include "llselectmgr.h"
#include "llslurl.h"
#include "llspinctrl.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lltexturectrl.h"
#include "lluiconstants.h"
#include "lluictrlfactory.h"
#include "llviewertexturelist.h"        // LLUIImageList
#include "llviewermenufile.h" // <FS:PP> Ban and access lists export/import
#include "llviewermessage.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llviewertexteditor.h"
#include "llviewerwindow.h"
#include "llviewercontrol.h"
#include "roles_constants.h"
#include "lltrans.h"
#include "llpanelexperiencelisteditor.h"
#include "llpanelexperiencepicker.h"
#include "llpanelenvironment.h"
#include "llexperiencecache.h"

#include "llgroupactions.h"
#include "llenvironment.h"
#include "llsdutil_math.h"
#include "llregionhandle.h"

#include "llworld.h" // <FS:Ansariel> For FIRE-1292
#include "llsdutil.h"
#include "llviewernetwork.h"
#include "fsnamelistavatarmenu.h"
#include "llfloaterauction.h"

const F64 COVENANT_REFRESH_TIME_SEC = 60.0f;

static std::string OWNER_ONLINE     = "0";
static std::string OWNER_OFFLINE    = "1";
static std::string OWNER_GROUP      = "2";
static std::string MATURITY         = "[MATURITY]";

// constants used in callbacks below - syntactic sugar.
static const bool BUY_GROUP_LAND = true;
static const bool BUY_PERSONAL_LAND = false;
LLPointer<LLParcelSelection> LLPanelLandGeneral::sSelectionForBuyPass = NULL;

// Statics
LLParcelSelectionObserver* LLFloaterLand::sObserver = NULL;
S32 LLFloaterLand::sLastTab = 0;

// Local classes
class LLParcelSelectionObserver : public LLParcelObserver
{
public:
    virtual void changed() { LLFloaterLand::refreshAll(); }
};

// class needed to get full access to textbox inside checkbox, because LLCheckBoxCtrl::setLabel() has string as its argument.
// It was introduced while implementing EXT-4706
class LLCheckBoxWithTBAcess : public LLCheckBoxCtrl
{
public:
    LLTextBox* getTextBox()
    {
        return mLabel;
    }
};


class LLPanelLandExperiences
    :   public LLPanel
{
public:
    LLPanelLandExperiences(LLSafeHandle<LLParcelSelection>& parcelp);
    virtual bool postBuild();
    void refresh();

    void experienceAdded(const LLUUID& id, U32 xp_type, U32 access_type);
    void experienceRemoved(const LLUUID& id, U32 access_type);
protected:
    LLPanelExperienceListEditor* setupList( const char* control_name, U32 xp_type, U32 access_type );
    void refreshPanel(LLPanelExperienceListEditor* panel, U32 xp_type);

    LLSafeHandle<LLParcelSelection>&    mParcel;


    LLPanelExperienceListEditor* mAllowed;
    LLPanelExperienceListEditor* mBlocked;
};


class LLPanelLandEnvironment
    : public LLPanelEnvironmentInfo
{
public:
                        LLPanelLandEnvironment(LLSafeHandle<LLParcelSelection>& parcelp);

    virtual bool        isRegion() const override { return false; }
    virtual bool        isLargeEnough() override
    {
        LLParcel *parcelp = mParcel->getParcel();
        return ((parcelp) ? (parcelp->getArea() >= MINIMUM_PARCEL_SIZE) : false);
    }

    virtual bool        postBuild() override;
    virtual void        refresh() override;

    virtual LLParcel *  getParcel() override;

    virtual bool        canEdit() override;
    virtual S32         getParcelId() override;

protected:
    virtual void        refreshFromSource() override;

    bool                isSameRegion();

    LLSafeHandle<LLParcelSelection> &   mParcel;
    S32                 mLastParcelId;
};


// inserts maturity info(icon and text) into target textbox
// names_floater - pointer to floater which contains strings with maturity icons filenames
// str_to_parse is string in format "txt1[MATURITY]txt2" where maturity icon and text will be inserted instead of [MATURITY]
void insert_maturity_into_textbox(LLTextBox* target_textbox, LLFloater* names_floater, std::string str_to_parse);

//---------------------------------------------------------------------------
// LLFloaterLand
//---------------------------------------------------------------------------

void send_parcel_select_objects(S32 parcel_local_id, U32 return_type,
                                uuid_list_t* return_ids = NULL)
{
    LLMessageSystem *msg = gMessageSystem;

    LLViewerRegion* region = LLViewerParcelMgr::getInstance()->getSelectionRegion();
    if (!region) return;

    // Since new highlight will be coming in, drop any highlights
    // that exist right now.
    LLSelectMgr::getInstance()->unhighlightAll();

    msg->newMessageFast(_PREHASH_ParcelSelectObjects);
    msg->nextBlockFast(_PREHASH_AgentData);
    msg->addUUIDFast(_PREHASH_AgentID,  gAgent.getID());
    msg->addUUIDFast(_PREHASH_SessionID,gAgent.getSessionID());
    msg->nextBlockFast(_PREHASH_ParcelData);
    msg->addS32Fast(_PREHASH_LocalID, parcel_local_id);
    msg->addU32Fast(_PREHASH_ReturnType, return_type);

    // Throw all return ids into the packet.
    // TODO: Check for too many ids.
    if (return_ids)
    {
        uuid_list_t::iterator end = return_ids->end();
        for (uuid_list_t::iterator it = return_ids->begin();
             it != end;
             ++it)
        {
            msg->nextBlockFast(_PREHASH_ReturnIDs);
            msg->addUUIDFast(_PREHASH_ReturnID, (*it));
        }
    }
    else
    {
        // Put in a null key so that the message is complete.
        msg->nextBlockFast(_PREHASH_ReturnIDs);
        msg->addUUIDFast(_PREHASH_ReturnID, LLUUID::null);
    }

    msg->sendReliable(region->getHost());
}

LLParcel* LLFloaterLand::getCurrentSelectedParcel()
{
    return mParcel->getParcel();
};

//static
LLPanelLandObjects* LLFloaterLand::getCurrentPanelLandObjects()
{
    LLFloaterLand* land_instance = LLFloaterReg::getTypedInstance<LLFloaterLand>("about_land");
    if(land_instance)
    {
        return land_instance->mPanelObjects;
    }
    else
    {
        return NULL;
    }
}

//static
LLPanelLandCovenant* LLFloaterLand::getCurrentPanelLandCovenant()
{
    LLFloaterLand* land_instance = LLFloaterReg::getTypedInstance<LLFloaterLand>("about_land");
    if(land_instance)
    {
        return land_instance->mPanelCovenant;
    }
    else
    {
        return NULL;
    }
}

// static
void LLFloaterLand::refreshAll()
{
    LLFloaterLand* land_instance = LLFloaterReg::findTypedInstance<LLFloaterLand>("about_land");
    if(land_instance)
    {
        land_instance->refresh();
    }
}

void LLFloaterLand::onOpen(const LLSD& key)
{
    // moved from triggering show instance in llviwermenu.cpp

    if (LLViewerParcelMgr::getInstance()->selectionEmpty())
    {
        LLViewerParcelMgr::getInstance()->selectParcelAt(gAgent.getPositionGlobal());
    }

    // Done automatically when the selected parcel's properties arrive
    // (and hence we have the local id).
    // LLViewerParcelMgr::getInstance()->sendParcelAccessListRequest(AL_ACCESS | AL_BAN | AL_RENTER);

    mParcel = LLViewerParcelMgr::getInstance()->getFloatingParcelSelection();

    // <FS:Ansariel> FIRE-17280: Requesting Experience access allow & block list breaks OpenSim
    LLViewerRegion* selected_region = LLViewerParcelMgr::getInstance()->getSelectionRegion();
    if (!selected_region || !selected_region->isCapabilityAvailable("RegionExperiences"))
    {
        mTabLand->removeTabPanel(mTabLand->getPanelByName("land_experiences_panel"));
    }
    // </FS:Ansariel>

    // Refresh even if not over a region so we don't get an
    // uninitialized dialog. The dialog is 0-region aware.
    refresh();
}

void LLFloaterLand::onVisibilityChanged(const LLSD& visible)
{
    if (!visible.asBoolean())
    {
        // Might have been showing owned objects
        LLSelectMgr::getInstance()->unhighlightAll();

        // Save which panel we had open
        sLastTab = mTabLand->getCurrentPanelIndex();
    }
}


LLFloaterLand::LLFloaterLand(const LLSD& seed)
:   LLFloater(seed)
{
    mFactoryMap["land_general_panel"] = LLCallbackMap(createPanelLandGeneral, this);
    mFactoryMap["land_covenant_panel"] = LLCallbackMap(createPanelLandCovenant, this);
    mFactoryMap["land_objects_panel"] = LLCallbackMap(createPanelLandObjects, this);
    mFactoryMap["land_options_panel"] = LLCallbackMap(createPanelLandOptions, this);
    mFactoryMap["land_audio_panel"] =   LLCallbackMap(createPanelLandAudio, this);
    mFactoryMap["land_media_panel"] =   LLCallbackMap(createPanelLandMedia, this);
    mFactoryMap["land_access_panel"] =  LLCallbackMap(createPanelLandAccess, this);
    mFactoryMap["land_experiences_panel"] = LLCallbackMap(createPanelLandExperiences, this);
    mFactoryMap["land_environment_panel"] = LLCallbackMap(createPanelLandEnvironment, this);

    sObserver = new LLParcelSelectionObserver();
    LLViewerParcelMgr::getInstance()->addObserver( sObserver );
}

bool LLFloaterLand::postBuild()
{
    setVisibleCallback(boost::bind(&LLFloaterLand::onVisibilityChanged, this, _2));

    LLTabContainer* tab = getChild<LLTabContainer>("landtab");

    mTabLand = (LLTabContainer*) tab;

    if (tab)
    {
        tab->selectTab(sLastTab);
    }

    return true;
}


// virtual
LLFloaterLand::~LLFloaterLand()
{
    LLViewerParcelMgr::getInstance()->removeObserver( sObserver );
    delete sObserver;
    sObserver = NULL;
}

// public
void LLFloaterLand::refresh()
{
    mPanelGeneral->refresh();
    mPanelObjects->refresh();
    mPanelOptions->refresh();
    mPanelAudio->refresh();
    mPanelMedia->refresh();
    mPanelAccess->refresh();
    mPanelCovenant->refresh();
    mPanelExperiences->refresh();
    mPanelEnvironment->refresh();
}



void* LLFloaterLand::createPanelLandGeneral(void* data)
{
    LLFloaterLand* self = (LLFloaterLand*)data;
    self->mPanelGeneral = new LLPanelLandGeneral(self->mParcel);
    return self->mPanelGeneral;
}

// static
void* LLFloaterLand::createPanelLandCovenant(void* data)
{
    LLFloaterLand* self = (LLFloaterLand*)data;
    self->mPanelCovenant = new LLPanelLandCovenant(self->mParcel);
    return self->mPanelCovenant;
}


// static
void* LLFloaterLand::createPanelLandObjects(void* data)
{
    LLFloaterLand* self = (LLFloaterLand*)data;
    self->mPanelObjects = new LLPanelLandObjects(self->mParcel);
    return self->mPanelObjects;
}

// static
void* LLFloaterLand::createPanelLandOptions(void* data)
{
    LLFloaterLand* self = (LLFloaterLand*)data;
    self->mPanelOptions = new LLPanelLandOptions(self->mParcel);
    return self->mPanelOptions;
}

// static
void* LLFloaterLand::createPanelLandAudio(void* data)
{
    LLFloaterLand* self = (LLFloaterLand*)data;
    self->mPanelAudio = new LLPanelLandAudio(self->mParcel);
    return self->mPanelAudio;
}

// static
void* LLFloaterLand::createPanelLandMedia(void* data)
{
    LLFloaterLand* self = (LLFloaterLand*)data;
    self->mPanelMedia = new LLPanelLandMedia(self->mParcel);
    return self->mPanelMedia;
}

// static
void* LLFloaterLand::createPanelLandAccess(void* data)
{
    LLFloaterLand* self = (LLFloaterLand*)data;
    self->mPanelAccess = new LLPanelLandAccess(self->mParcel);
    return self->mPanelAccess;
}

// static
void* LLFloaterLand::createPanelLandExperiences(void* data)
{
    LLFloaterLand* self = (LLFloaterLand*)data;
    self->mPanelExperiences = new LLPanelLandExperiences(self->mParcel);
    return self->mPanelExperiences;
}

//static
void* LLFloaterLand::createPanelLandEnvironment(void* data)
{
    LLFloaterLand* self = (LLFloaterLand*)data;
    self->mPanelEnvironment = new LLPanelLandEnvironment(self->mParcel);
    return self->mPanelEnvironment;
}


//---------------------------------------------------------------------------
// LLPanelLandGeneral
//---------------------------------------------------------------------------


LLPanelLandGeneral::LLPanelLandGeneral(LLParcelSelectionHandle& parcel)
:   LLPanel(),
    mUncheckedSell(false),
    mParcel(parcel),
    mLastParcelLocalID(0)
{
}

bool LLPanelLandGeneral::postBuild()
{
    mEditName = getChild<LLLineEditor>("Name");
    mEditName->setCommitCallback(onCommitAny, this);
    getChild<LLLineEditor>("Name")->setPrevalidate(LLTextValidate::validateASCIIPrintableNoPipe);

    mEditUUID = getChild<LLLineEditor>("UUID");
    mEditUUID->setEnabled(false);

    mEditDesc = getChild<LLTextEditor>("Description");
    mEditDesc->setCommitOnFocusLost(true);
    mEditDesc->setCommitCallback(onCommitAny, this);
    mEditDesc->setContentTrusted(false);
    // No prevalidate function - historically the prevalidate function was broken,
    // allowing residents to put in characters like U+2661 WHITE HEART SUIT, so
    // preserve that ability.

    mTextSalePending = getChild<LLTextBox>("SalePending");
    mTextOwnerLabel = getChild<LLTextBox>("Owner:");
    mTextOwner = getChild<LLTextBox>("OwnerText");
    mTextOwner->setIsFriendCallback(LLAvatarActions::isFriend);

    mContentRating = getChild<LLTextBox>("ContentRatingText");
    mLandType = getChild<LLTextBox>("LandTypeText");

    // <FS:Ansariel> Doesn't exists as of 2014-04-14
    //mBtnProfile = getChild<LLButton>("Profile...");
    //mBtnProfile->setClickedCallback(boost::bind(&LLPanelLandGeneral::onClickProfile, this));


    mTextGroupLabel = getChild<LLTextBox>("Group:");
    mTextGroup = getChild<LLTextBox>("GroupText");


    mBtnSetGroup = getChild<LLButton>("Set...");
    mBtnSetGroup->setCommitCallback(boost::bind(&LLPanelLandGeneral::onClickSetGroup, this));


    mCheckDeedToGroup = getChild<LLCheckBoxCtrl>( "check deed");
    childSetCommitCallback("check deed", onCommitAny, this);


    mBtnDeedToGroup = getChild<LLButton>("Deed...");
    mBtnDeedToGroup->setClickedCallback(onClickDeed, this);


    mCheckContributeWithDeed = getChild<LLCheckBoxCtrl>( "check contrib");
    childSetCommitCallback("check contrib", onCommitAny, this);



    mSaleInfoNotForSale = getChild<LLTextBox>("Not for sale.");

    mSaleInfoForSale1 = getChild<LLTextBox>("For Sale: Price L$[PRICE].");


    mBtnSellLand = getChild<LLButton>("Sell Land...");
    mBtnSellLand->setClickedCallback(onClickSellLand, this);

    mSaleInfoForSale2 = getChild<LLTextBox>("For sale to");

    mSaleInfoForSaleObjects = getChild<LLTextBox>("Sell with landowners objects in parcel.");

    mSaleInfoForSaleNoObjects = getChild<LLTextBox>("Selling with no objects in parcel.");


    mBtnStopSellLand = getChild<LLButton>("Cancel Land Sale");
    mBtnStopSellLand->setClickedCallback(onClickStopSellLand, this);


    mTextClaimDateLabel = getChild<LLTextBox>("Claimed:");
    mTextClaimDate = getChild<LLTextBox>("DateClaimText");


    mTextPriceLabel = getChild<LLTextBox>("PriceLabel");
    mTextPrice = getChild<LLTextBox>("PriceText");


    mTextDwell = getChild<LLTextBox>("DwellText");

    mBtnBuyLand = getChild<LLButton>("Buy Land...");
    mBtnBuyLand->setClickedCallback(onClickBuyLand, (void*)&BUY_PERSONAL_LAND);


    mBtnBuyGroupLand = getChild<LLButton>("Buy For Group...");
    mBtnBuyGroupLand->setClickedCallback(onClickBuyLand, (void*)&BUY_GROUP_LAND);


    mBtnBuyPass = getChild<LLButton>("Buy Pass...");
    mBtnBuyPass->setClickedCallback(onClickBuyPass, this);

    mBtnReleaseLand = getChild<LLButton>("Abandon Land...");
    mBtnReleaseLand->setClickedCallback(onClickRelease, NULL);

    mBtnReclaimLand = getChild<LLButton>("Reclaim Land...");
    mBtnReclaimLand->setClickedCallback(onClickReclaim, NULL);

    mBtnStartAuction = getChild<LLButton>("Linden Sale...");
    mBtnStartAuction->setClickedCallback(onClickStartAuction, this);
    mBtnStartAuction->setEnabled(LLGridManager::instance().isInSecondLife()); // <FS:Ansariel> Restore land auction floater for OpenSim

    mBtnScriptLimits = getChild<LLButton>("Scripts...");

    if(gDisconnected)
    {
        return true;
    }

    // note: on region change this will not be re checked, should not matter on Agni as
    // 99% of the time all regions will return the same caps. In case of an erroneous setting
    // to enabled the floater will just throw an error when trying to get it's cap
    std::string url = gAgent.getRegionCapability("LandResources");
    if (!url.empty())
    {
        if(mBtnScriptLimits)
        {
            mBtnScriptLimits->setClickedCallback(onClickScriptLimits, this);
        }
    }
    else
    {
        if(mBtnScriptLimits)
        {
            mBtnScriptLimits->setVisible(false);
        }
    }

    return true;
}


// virtual
LLPanelLandGeneral::~LLPanelLandGeneral()
{ }


// public
void LLPanelLandGeneral::refresh()
{
    mEditName->setEnabled(false);
    mEditName->setText(LLStringUtil::null);

    mEditUUID->setText(LLStringUtil::null);

    mEditDesc->setEnabled(false);
    mEditDesc->setText(getString("no_selection_text"));

    mTextSalePending->setText(LLStringUtil::null);
    mTextSalePending->setEnabled(false);

    mBtnDeedToGroup->setEnabled(false);
    mBtnSetGroup->setEnabled(false);
    mBtnStartAuction->setEnabled(false);

    mCheckDeedToGroup   ->set(false);
    mCheckDeedToGroup   ->setEnabled(false);
    mCheckContributeWithDeed->set(false);
    mCheckContributeWithDeed->setEnabled(false);

    mTextOwner->setText(LLStringUtil::null);
    mContentRating->setText(LLStringUtil::null);
    mLandType->setText(LLStringUtil::null);
    // <FS:Ansariel> Doesn't exists as of 2014-04-14
    //mBtnProfile->setLabel(getString("profile_text"));
    //mBtnProfile->setEnabled(false);

    mTextClaimDate->setText(LLStringUtil::null);
    mTextGroup->setText(LLStringUtil::null);
    mTextPrice->setText(LLStringUtil::null);

    mSaleInfoForSale1->setVisible(false);
    mSaleInfoForSale2->setVisible(false);
    mSaleInfoForSaleObjects->setVisible(false);
    mSaleInfoForSaleNoObjects->setVisible(false);
    mSaleInfoNotForSale->setVisible(false);
    mBtnSellLand->setVisible(false);
    mBtnStopSellLand->setVisible(false);

    mTextPriceLabel->setText(LLStringUtil::null);
    mTextDwell->setText(LLStringUtil::null);

    mBtnBuyLand->setEnabled(false);
    mBtnScriptLimits->setEnabled(false);
    mBtnBuyGroupLand->setEnabled(false);
    mBtnReleaseLand->setEnabled(false);
    mBtnReclaimLand->setEnabled(false);
    mBtnBuyPass->setEnabled(false);

    if(gDisconnected)
    {
        return;
    }

    mBtnStartAuction->setVisible(gAgent.isGodlike());
    bool region_owner = false;
    LLViewerRegion* regionp = LLViewerParcelMgr::getInstance()->getSelectionRegion();
    if(regionp && (regionp->getOwner() == gAgent.getID()))
    {
        region_owner = true;
        mBtnReleaseLand->setVisible(false);
        mBtnReclaimLand->setVisible(true);
    }
    else
    {
        mBtnReleaseLand->setVisible(true);
        mBtnReclaimLand->setVisible(false);
    }
    LLParcel *parcel = mParcel->getParcel();
    if (parcel)
    {
        // something selected, hooray!
        bool is_leased = (LLParcel::OS_LEASED == parcel->getOwnershipStatus());
        bool region_xfer = false;
        if(regionp
           && !(regionp->getRegionFlag(REGION_FLAGS_BLOCK_LAND_RESELL)))
        {
            region_xfer = true;
        }

        if (regionp)
        {
            insert_maturity_into_textbox(mContentRating, gFloaterView->getParentFloater(this), MATURITY);
            mLandType->setText(regionp->getLocalizedSimProductName());
        }

        // estate owner/manager cannot edit other parts of the parcel
        bool estate_manager_sellable = !parcel->getAuctionID()
                                        && gAgent.canManageEstate()
                                        // estate manager/owner can only sell parcels owned by estate owner
                                        && regionp
                                        && (parcel->getOwnerID() == regionp->getOwner());
        bool owner_sellable = region_xfer && !parcel->getAuctionID()
                            && LLViewerParcelMgr::isParcelModifiableByAgent(
                                        parcel, GP_LAND_SET_SALE_INFO);
        bool can_be_sold = owner_sellable || estate_manager_sellable;

        const LLUUID &owner_id = parcel->getOwnerID();
        bool is_public = parcel->isPublic();

        // Is it owned?
        if (is_public)
        {
            mTextSalePending->setText(LLStringUtil::null);
            mTextSalePending->setEnabled(false);
            mTextOwner->setText(getString("public_text"));
            mTextOwner->setEnabled(false);
            // <FS:Ansariel> Doesn't exists as of 2014-04-14
            //mBtnProfile->setEnabled(false);
            mTextClaimDate->setText(LLStringUtil::null);
            mTextClaimDate->setEnabled(false);
            mTextGroup->setText(getString("none_text"));
            mTextGroup->setEnabled(false);
            mBtnStartAuction->setEnabled(false);
        }
        else
        {
            if(!is_leased && (owner_id == gAgent.getID()))
            {
                mTextSalePending->setText(getString("need_tier_to_modify"));
                mTextSalePending->setEnabled(true);
            }
            else if(parcel->getAuctionID())
            {
                mTextSalePending->setText(getString("auction_id_text"));
                mTextSalePending->setTextArg("[ID]", llformat("%u", parcel->getAuctionID()));
                mTextSalePending->setEnabled(true);
            }
            else
            {
                // not the owner, or it is leased
                mTextSalePending->setText(LLStringUtil::null);
                mTextSalePending->setEnabled(false);
            }
            //refreshNames();
            mTextOwner->setEnabled(true);

            // We support both group and personal profiles
            // <FS:Ansariel> Doesn't exists as of 2014-04-14
            //mBtnProfile->setEnabled(true);

            if (parcel->getGroupID().isNull())
            {
                // Not group owned, so "Profile"
                // <FS:Ansariel> Doesn't exists as of 2014-04-14
                //mBtnProfile->setLabel(getString("profile_text"));

                mTextGroup->setText(getString("none_text"));
                mTextGroup->setEnabled(false);
            }
            else
            {
                // Group owned, so "Info"
                // <FS:Ansariel> Doesn't exists as of 2014-04-14
                //mBtnProfile->setLabel(getString("info_text"));

                //mTextGroup->setText("HIPPOS!");//parcel->getGroupName());
                mTextGroup->setEnabled(true);
            }

            // Display claim date
            time_t claim_date = parcel->getClaimDate();
            std::string claim_date_str = getString("time_stamp_template");
            LLSD substitution;
            substitution["datetime"] = (S32) claim_date;
            LLStringUtil::format (claim_date_str, substitution);
            mTextClaimDate->setText(claim_date_str);
            mTextClaimDate->setEnabled(is_leased);

            bool enable_auction = (gAgent.getGodLevel() >= GOD_LIAISON)
                                  && (owner_id == GOVERNOR_LINDEN_ID)
                                  && (parcel->getAuctionID() == 0);
            mBtnStartAuction->setEnabled(enable_auction);
        }

        // Display options
        bool can_edit_identity = LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_LAND_CHANGE_IDENTITY);
        mEditName->setEnabled(can_edit_identity);
        mEditDesc->setEnabled(can_edit_identity);
        mEditDesc->setParseURLs(!can_edit_identity);

        bool can_edit_agent_only = LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_NO_POWERS);
        mBtnSetGroup->setEnabled(can_edit_agent_only && !parcel->getIsGroupOwned());

        const LLUUID& group_id = parcel->getGroupID();

        // Can only allow deeding if you own it and it's got a group.
        bool enable_deed = (owner_id == gAgent.getID()
                            && group_id.notNull()
                            && gAgent.isInGroup(group_id));
        // You don't need special powers to allow your object to
        // be deeded to the group.
        mCheckDeedToGroup->setEnabled(enable_deed);
        mCheckDeedToGroup->set( parcel->getAllowDeedToGroup() );
        mCheckContributeWithDeed->setEnabled(enable_deed && parcel->getAllowDeedToGroup());
        mCheckContributeWithDeed->set(parcel->getContributeWithDeed());

        // Actually doing the deeding requires you to have GP_LAND_DEED
        // powers in the group.
        bool can_deed = gAgent.hasPowerInGroup(group_id, GP_LAND_DEED);
        mBtnDeedToGroup->setEnabled(   parcel->getAllowDeedToGroup()
                                    && group_id.notNull()
                                    && can_deed
                                    && !parcel->getIsGroupOwned()
                                    );

        mEditName->setText( parcel->getName() );
        mEditDesc->setText( parcel->getDesc() );
        mEditUUID->setText(LLStringUtil::null);

        bool for_sale = parcel->getForSale();

        mBtnSellLand->setVisible(false);
        mBtnStopSellLand->setVisible(false);

        // show pricing information
        S32 area;
        S32 claim_price;
        S32 rent_price;
        F32 dwell = DWELL_NAN;
        LLViewerParcelMgr::getInstance()->getDisplayInfo(&area,
                                 &claim_price,
                                 &rent_price,
                                 &for_sale,
                                 &dwell);
        // Area
        LLUIString price = getString("area_size_text");
        price.setArg("[AREA]", llformat("%d",area));
        mTextPriceLabel->setText(getString("area_text"));
        mTextPrice->setText(price.getString());

        if (dwell == DWELL_NAN)
        {
            mTextDwell->setText(LLTrans::getString("LoadingData"));
        }
        else
        {
            mTextDwell->setText(llformat("%.0f", dwell));
        }

        if (for_sale)
        {
            mSaleInfoForSale1->setVisible(true);
            mSaleInfoForSale2->setVisible(true);
            if (parcel->getSellWithObjects())
            {
                mSaleInfoForSaleObjects->setVisible(true);
                mSaleInfoForSaleNoObjects->setVisible(false);
            }
            else
            {
                mSaleInfoForSaleObjects->setVisible(false);
                mSaleInfoForSaleNoObjects->setVisible(true);
            }
            mSaleInfoNotForSale->setVisible(false);

            F32 cost_per_sqm = 0.0f;
            if (area > 0)
            {
                cost_per_sqm = (F32)parcel->getSalePrice() / (F32)area;
            }

            S32 price = parcel->getSalePrice();
            mSaleInfoForSale1->setTextArg("[PRICE]", LLResMgr::getInstance()->getMonetaryString(price));
            mSaleInfoForSale1->setTextArg("[PRICE_PER_SQM]", llformat("%.1f", cost_per_sqm));
            if (can_be_sold)
            {
                mBtnStopSellLand->setVisible(true);
            }
        }
        else
        {
            mSaleInfoForSale1->setVisible(false);
            mSaleInfoForSale2->setVisible(false);
            mSaleInfoForSaleObjects->setVisible(false);
            mSaleInfoForSaleNoObjects->setVisible(false);
            mSaleInfoNotForSale->setVisible(true);
            if (can_be_sold)
            {
                mBtnSellLand->setVisible(true);
            }
        }

        refreshNames();

        mBtnBuyLand->setEnabled(
            LLViewerParcelMgr::getInstance()->canAgentBuyParcel(parcel, false));
        mBtnScriptLimits->setEnabled(true);
//          LLViewerParcelMgr::getInstance()->canAgentBuyParcel(parcel, false));
        mBtnBuyGroupLand->setEnabled(
            LLViewerParcelMgr::getInstance()->canAgentBuyParcel(parcel, true));

        if(region_owner)
        {
            mBtnReclaimLand->setEnabled(
                !is_public && (parcel->getOwnerID() != gAgent.getID()));
        }
        else
        {
            bool is_owner_release = LLViewerParcelMgr::isParcelOwnedByAgent(parcel, GP_LAND_RELEASE);
            bool is_manager_release = (gAgent.canManageEstate() &&
                                    regionp &&
                                    (parcel->getOwnerID() != regionp->getOwner()));
            bool can_release = is_owner_release || is_manager_release;
            mBtnReleaseLand->setEnabled( can_release );
        }

        bool use_pass = parcel->getOwnerID()!= gAgent.getID() && parcel->getParcelFlag(PF_USE_PASS_LIST) && !LLViewerParcelMgr::getInstance()->isCollisionBanned();;
        mBtnBuyPass->setEnabled(use_pass);

        // <Ansariel> Retrieve parcel UUID. We need to ask the itself for the
        //            parcel UUID, because LLParcel gets initialized with a
        //            null ID.
        //            Following part is basically copied from
        //            LLPanelScriptLimitsRegionMemory::StartRequestChain()
        if (regionp && gAgent.getRegion() && gAgent.getRegion()->getRegionID() == regionp->getRegionID() &&
            (mLastParcelLocalID == 0 || mLastParcelLocalID != parcel->getLocalID()) )
        {
            mLastParcelLocalID = parcel->getLocalID();
            std::string url = regionp->getCapability("RemoteParcelRequest");
            if (!url.empty())
            {
                LLUUID region_id = regionp->getRegionID();
                // <FS:Beq> FIRE-31213 - from Ubit Umarov - Correct position calcs to work with var regions
                // LLVector3d pos_global = regionp->getCenterGlobal();
                LLVector3d pos_global = regionp->getOriginGlobal();
                // </FS:Beq>
                LLRemoteParcelInfoProcessor::instance().requestRegionParcelInfo( url, region_id, parcel->getCenterpoint(), pos_global, getObserverHandle() );
            }
            else
            {
                LL_WARNS() << "RemoteParcelRequest not available. Cannot request parcel ID" << LL_ENDL;
                mEditUUID->setText(getString("error_resolving_uuid"));
            }
        }
        // </Ansariel>
    }
}

// public
void LLPanelLandGeneral::refreshNames()
{
    LLParcel *parcel = mParcel->getParcel();
    if (!parcel)
    {
        mTextOwner->setText(LLStringUtil::null);
        mTextGroup->setText(LLStringUtil::null);
        return;
    }

    std::string owner;
    if (parcel->getIsGroupOwned())
    {
        owner = getString("group_owned_text");
    }
    else
    {
        // Figure out the owner's name
        owner = LLSLURL("agent", parcel->getOwnerID(), "inspect").getSLURLString();
    }

    if(LLParcel::OS_LEASE_PENDING == parcel->getOwnershipStatus())
    {
        owner += getString("sale_pending_text");
    }
    mTextOwner->setText(owner);

    std::string group;
    if (!parcel->getGroupID().isNull())
    {
        group = LLSLURL("group", parcel->getGroupID(), "inspect").getSLURLString();
    }
    mTextGroup->setText(group);

    if (parcel->getForSale())
    {
        const LLUUID& auth_buyer_id = parcel->getAuthorizedBuyerID();
        if(auth_buyer_id.notNull())
        {
          std::string name;
          name = LLSLURL("agent", auth_buyer_id, "inspect").getSLURLString();
          mSaleInfoForSale2->setTextArg("[BUYER]", name);
        }
        else
        {
            mSaleInfoForSale2->setTextArg("[BUYER]", getString("anyone"));
        }
    }
}


// virtual
void LLPanelLandGeneral::draw()
{
    LLPanel::draw();
}

void LLPanelLandGeneral::onClickSetGroup()
{
    LLFloater* parent_floater = gFloaterView->getParentFloater(this);

    LLFloaterGroupPicker* fg =  LLFloaterReg::showTypedInstance<LLFloaterGroupPicker>("group_picker", LLSD(gAgent.getID()));
    if (fg)
    {
        fg->setSelectGroupCallback( boost::bind(&LLPanelLandGeneral::setGroup, this, _1 ));
        if (parent_floater)
        {
            LLRect new_rect = gFloaterView->findNeighboringPosition(parent_floater, fg);
            fg->setOrigin(new_rect.mLeft, new_rect.mBottom);
            parent_floater->addDependentFloater(fg);
        }
    }
}

// <FS:Ansariel> Doesn't exists as of 2014-04-14
//void LLPanelLandGeneral::onClickProfile()
//{
//  LLParcel* parcel = mParcel->getParcel();
//  if (!parcel) return;
//
//  if (parcel->getIsGroupOwned())
//  {
//      const LLUUID& group_id = parcel->getGroupID();
//      LLGroupActions::show(group_id);
//  }
//  else
//  {
//      const LLUUID& avatar_id = parcel->getOwnerID();
//      LLAvatarActions::showProfile(avatar_id);
//  }
//}
// </FS:Ansariel>

// public
void LLPanelLandGeneral::setGroup(const LLUUID& group_id)
{
    LLParcel* parcel = mParcel->getParcel();
    if (!parcel) return;

    // Set parcel properties and send message
    parcel->setGroupID(group_id);
    //parcel->setGroupName(group_name);
    //mTextGroup->setText(group_name);

    // Send update
    LLViewerParcelMgr::getInstance()->sendParcelPropertiesUpdate(parcel);

    // Update UI
    refresh();
}

// static
void LLPanelLandGeneral::onClickBuyLand(void* data)
{
    bool* for_group = (bool*)data;
    LLViewerParcelMgr::getInstance()->startBuyLand(*for_group);
}

// static
void LLPanelLandGeneral::onClickScriptLimits(void* data)
{
    LLPanelLandGeneral* panelp = (LLPanelLandGeneral*)data;
    LLParcel* parcel = panelp->mParcel->getParcel();
    if(parcel != NULL)
    {
        LLFloaterReg::showInstance("script_limits");
    }
}

// static
void LLPanelLandGeneral::onClickDeed(void*)
{
    //LLParcel* parcel = mParcel->getParcel();
    //if (parcel)
    //{
    LLViewerParcelMgr::getInstance()->startDeedLandToGroup();
    //}
}

// static
void LLPanelLandGeneral::onClickRelease(void*)
{
    LLViewerParcelMgr::getInstance()->startReleaseLand();
}

// static
void LLPanelLandGeneral::onClickReclaim(void*)
{
    LL_DEBUGS() << "LLPanelLandGeneral::onClickReclaim()" << LL_ENDL;
    LLViewerParcelMgr::getInstance()->reclaimParcel();
}

// static
bool LLPanelLandGeneral::enableBuyPass(void* data)
{
    LLPanelLandGeneral* panelp = (LLPanelLandGeneral*)data;
    LLParcel* parcel = panelp != NULL ? panelp->mParcel->getParcel() : LLViewerParcelMgr::getInstance()->getParcelSelection()->getParcel();
    return (parcel != NULL) && (parcel->getParcelFlag(PF_USE_PASS_LIST) && !LLViewerParcelMgr::getInstance()->isCollisionBanned());
}


// static
void LLPanelLandGeneral::onClickBuyPass(void* data)
{
    LLPanelLandGeneral* panelp = (LLPanelLandGeneral*)data;
    LLParcel* parcel = panelp != NULL ? panelp->mParcel->getParcel() : LLViewerParcelMgr::getInstance()->getParcelSelection()->getParcel();

    if (!parcel) return;

    S32 pass_price = parcel->getPassPrice();
    std::string parcel_name = parcel->getName();
    F32 pass_hours = parcel->getPassHours();

    std::string cost, time;
    cost = llformat("%d", pass_price);
    time = llformat("%.2f", pass_hours);

    LLSD args;
    args["COST"] = cost;
    args["PARCEL_NAME"] = parcel_name;
    args["TIME"] = time;

    // creating pointer on selection to avoid deselection of parcel until we are done with buying pass (EXT-6464)
    sSelectionForBuyPass = LLViewerParcelMgr::getInstance()->getParcelSelection();
    LLNotificationsUtil::add("LandBuyPass", args, LLSD(), cbBuyPass);
}

// static
void LLPanelLandGeneral::onClickStartAuction(void* data)
{
    LLPanelLandGeneral* panelp = (LLPanelLandGeneral*)data;
    LLParcel* parcelp = panelp->mParcel->getParcel();
    if(parcelp)
    {
        if(parcelp->getForSale())
        {
            LLNotificationsUtil::add("CannotStartAuctionAlreadyForSale");
        }
        else
        {
            //LLFloaterAuction::showInstance();
            LLFloaterReg::showInstance("auction");
        }
    }
}

// static
bool LLPanelLandGeneral::cbBuyPass(const LLSD& notification, const LLSD& response)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    if (0 == option)
    {
        // User clicked OK
        LLViewerParcelMgr::getInstance()->buyPass();
    }
    // we are done with buying pass, additional selection is no longer needed
    sSelectionForBuyPass = NULL;
    return false;
}

// static
void LLPanelLandGeneral::onCommitAny(LLUICtrl *ctrl, void *userdata)
{
    LLPanelLandGeneral *panelp = (LLPanelLandGeneral *)userdata;

    LLParcel* parcel = panelp->mParcel->getParcel();
    if (!parcel)
    {
        return;
    }

    // Extract data from UI
    std::string name = panelp->mEditName->getText();
    std::string desc = panelp->mEditDesc->getText();

    // Valid data from UI

    // Stuff data into selected parcel
    parcel->setName(name);
    parcel->setDesc(desc);

    bool allow_deed_to_group= panelp->mCheckDeedToGroup->get();
    bool contribute_with_deed = panelp->mCheckContributeWithDeed->get();

    parcel->setParcelFlag(PF_ALLOW_DEED_TO_GROUP, allow_deed_to_group);
    parcel->setContributeWithDeed(contribute_with_deed);

    // Send update to server
    LLViewerParcelMgr::getInstance()->sendParcelPropertiesUpdate( parcel );

    // Might have changed properties, so let's redraw!
    panelp->refresh();
}

// static
void LLPanelLandGeneral::onClickSellLand(void* data)
{
    LLViewerParcelMgr::getInstance()->startSellLand();
    LLPanelLandGeneral *panelp = (LLPanelLandGeneral *)data;
    panelp->refresh();
}

// static
void LLPanelLandGeneral::onClickStopSellLand(void* data)
{
    LLPanelLandGeneral* panelp = (LLPanelLandGeneral*)data;
    LLParcel* parcel = panelp->mParcel->getParcel();

    parcel->setParcelFlag(PF_FOR_SALE, false);
    parcel->setSalePrice(0);
    parcel->setAuthorizedBuyerID(LLUUID::null);

    LLViewerParcelMgr::getInstance()->sendParcelPropertiesUpdate(parcel);
}

// <Ansariel> For retrieving the parcel UUID:
//            Implementation of LLRemoteParcelInfoObserver interface
void LLPanelLandGeneral::processParcelInfo(const LLParcelData& parcel_data)
{
}

void LLPanelLandGeneral::setParcelID(const LLUUID& parcel_id)
{
    // No sanity check needed here. LLRemoteParcelRequestResponder
    // will do that for us!
    mEditUUID->setText(parcel_id.asString());
}

// virtual
void LLPanelLandGeneral::setErrorStatus(S32 status, const std::string& reason)
{
    mEditUUID->setText(getString("error_resolving_uuid"));
    mLastParcelLocalID = 0;
    LL_WARNS() << "Can't handle remote parcel request."<< " Http Status: "<< status << ". Reason : "<< reason<<LL_ENDL;
}
// </Ansariel>

//---------------------------------------------------------------------------
// LLPanelLandObjects
//---------------------------------------------------------------------------
LLPanelLandObjects::LLPanelLandObjects(LLParcelSelectionHandle& parcel)
    :   LLPanel(),

        mParcel(parcel),
        mParcelObjectBonus(NULL),
        mSWTotalObjects(NULL),
        mObjectContribution(NULL),
        mTotalObjects(NULL),
        mOwnerObjects(NULL),
        mBtnShowOwnerObjects(NULL),
        mBtnReturnOwnerObjects(NULL),
        mGroupObjects(NULL),
        mBtnShowGroupObjects(NULL),
        mBtnReturnGroupObjects(NULL),
        mOtherObjects(NULL),
        mBtnShowOtherObjects(NULL),
        mBtnReturnOtherObjects(NULL),
        mSelectedObjects(NULL),
        mCleanOtherObjectsTime(NULL),
        mOtherTime(0),
        mBtnRefresh(NULL),
        mBtnReturnOwnerList(NULL),
        mOwnerList(NULL),
        mFirstReply(true),
        mSelectedCount(0),
        mSelectedIsGroup(false)
{
}



bool LLPanelLandObjects::postBuild()
{

    mFirstReply = true;
    mParcelObjectBonus = getChild<LLTextBox>("parcel_object_bonus");
    mSWTotalObjects = getChild<LLTextBox>("objects_available");
    mObjectContribution = getChild<LLTextBox>("object_contrib_text");
    mTotalObjects = getChild<LLTextBox>("total_objects_text");
    mOwnerObjects = getChild<LLTextBox>("owner_objects_text");

    mBtnShowOwnerObjects = getChild<LLButton>("ShowOwner");
    mBtnShowOwnerObjects->setClickedCallback(onClickShowOwnerObjects, this);

    mBtnReturnOwnerObjects = getChild<LLButton>("ReturnOwner...");
    mBtnReturnOwnerObjects->setClickedCallback(onClickReturnOwnerObjects, this);

    mGroupObjects = getChild<LLTextBox>("group_objects_text");
    mBtnShowGroupObjects = getChild<LLButton>("ShowGroup");
    mBtnShowGroupObjects->setClickedCallback(onClickShowGroupObjects, this);

    mBtnReturnGroupObjects = getChild<LLButton>("ReturnGroup...");
    mBtnReturnGroupObjects->setClickedCallback(onClickReturnGroupObjects, this);

    mOtherObjects = getChild<LLTextBox>("other_objects_text");
    mBtnShowOtherObjects = getChild<LLButton>("ShowOther");
    mBtnShowOtherObjects->setClickedCallback(onClickShowOtherObjects, this);

    mBtnReturnOtherObjects = getChild<LLButton>("ReturnOther...");
    mBtnReturnOtherObjects->setClickedCallback(onClickReturnOtherObjects, this);

    mSelectedObjects = getChild<LLTextBox>("selected_objects_text");
    mCleanOtherObjectsTime = getChild<LLLineEditor>("clean other time");

    mCleanOtherObjectsTime->setFocusLostCallback(boost::bind(onLostFocus, _1, this));
    mCleanOtherObjectsTime->setCommitCallback(onCommitClean, this);
    getChild<LLLineEditor>("clean other time")->setPrevalidate(LLTextValidate::validateNonNegativeS32);

    mBtnRefresh = getChild<LLButton>("Refresh List");
    mBtnRefresh->setClickedCallback(onClickRefresh, this);

    mBtnReturnOwnerList = getChild<LLButton>("Return objects...");
    mBtnReturnOwnerList->setClickedCallback(onClickReturnOwnerList, this);

    mIconAvatarOnline = LLUIImageList::getInstance()->getUIImage("icon_avatar_online.tga", 0);
    mIconAvatarOffline = LLUIImageList::getInstance()->getUIImage("icon_avatar_offline.tga", 0);
    mIconGroup = LLUIImageList::getInstance()->getUIImage("icon_group.tga", 0);

    mOwnerList = getChild<LLNameListCtrl>("owner list");
    mOwnerList->setIsFriendCallback(LLAvatarActions::isFriend);
    mOwnerList->sortByColumnIndex(3, false);
    childSetCommitCallback("owner list", onCommitList, this);
    mOwnerList->setDoubleClickCallback(onDoubleClickOwner, this);
    // <FS:Ansariel> Special Firestorm menu also allowing multi-select action
    //mOwnerList->setContextMenu(LLScrollListCtrl::MENU_AVATAR);
    mOwnerList->setContextMenu(&gFSNameListAvatarMenu);
    // </FS:Ansariel>

    return true;
}




// virtual
LLPanelLandObjects::~LLPanelLandObjects()
{ }

// static
void LLPanelLandObjects::onDoubleClickOwner(void *userdata)
{
    LLPanelLandObjects *self = (LLPanelLandObjects *)userdata;

    LLScrollListItem* item = self->mOwnerList->getFirstSelected();
    if (item)
    {
        LLUUID owner_id = item->getUUID();
        // Look up the selected name, for future dialog box use.
        const LLScrollListCell* cell;
        cell = item->getColumn(1);
        if (!cell)
        {
            return;
        }
        // Is this a group?
        bool is_group = cell->getValue().asString() == OWNER_GROUP;
        if (is_group)
        {
            LLGroupActions::show(owner_id);
        }
        else
        {
            LLAvatarActions::showProfile(owner_id);
        }
    }
}

// public
void LLPanelLandObjects::refresh()
{
    LLParcel *parcel = mParcel->getParcel();

    mBtnShowOwnerObjects->setEnabled(false);
    mBtnShowGroupObjects->setEnabled(false);
    mBtnShowOtherObjects->setEnabled(false);
    mBtnReturnOwnerObjects->setEnabled(false);
    mBtnReturnGroupObjects->setEnabled(false);
    mBtnReturnOtherObjects->setEnabled(false);
    mCleanOtherObjectsTime->setEnabled(false);
    mBtnRefresh->           setEnabled(false);
    mBtnReturnOwnerList->   setEnabled(false);

    mSelectedOwners.clear();
    mOwnerList->deleteAllItems();
    mOwnerList->setEnabled(false);

    if (!parcel || gDisconnected)
    {
        mSWTotalObjects->setTextArg("[COUNT]", llformat("%d", 0));
        mSWTotalObjects->setTextArg("[TOTAL]", llformat("%d", 0));
        mSWTotalObjects->setTextArg("[AVAILABLE]", llformat("%d", 0));
        mObjectContribution->setTextArg("[COUNT]", llformat("%d", 0));
        mTotalObjects->setTextArg("[COUNT]", llformat("%d", 0));
        mOwnerObjects->setTextArg("[COUNT]", llformat("%d", 0));
        mGroupObjects->setTextArg("[COUNT]", llformat("%d", 0));
        mOtherObjects->setTextArg("[COUNT]", llformat("%d", 0));
        mSelectedObjects->setTextArg("[COUNT]", llformat("%d", 0));
    }
    else
    {
        S32 sw_max = parcel->getSimWideMaxPrimCapacity();
        S32 sw_total = parcel->getSimWidePrimCount();
        S32 max = ll_round(parcel->getMaxPrimCapacity() * parcel->getParcelPrimBonus());
        S32 total = parcel->getPrimCount();
        S32 owned = parcel->getOwnerPrimCount();
        S32 group = parcel->getGroupPrimCount();
        S32 other = parcel->getOtherPrimCount();
        S32 selected = parcel->getSelectedPrimCount();
        F32 parcel_object_bonus = parcel->getParcelPrimBonus();
        mOtherTime = parcel->getCleanOtherTime();

        // Can't have more than region max tasks, regardless of parcel
        // object bonus factor.
        LLViewerRegion* region = LLViewerParcelMgr::getInstance()->getSelectionRegion();
        if (region)
        {
            S32 max_tasks_per_region = (S32)region->getMaxTasks();
            sw_max = llmin(sw_max, max_tasks_per_region);
            max = llmin(max, max_tasks_per_region);
        }

        if (parcel_object_bonus != 1.0f)
        {
            mParcelObjectBonus->setVisible(true);
            mParcelObjectBonus->setTextArg("[BONUS]", llformat("%.2f", parcel_object_bonus));
        }
        else
        {
            mParcelObjectBonus->setVisible(false);
        }

        if (sw_total > sw_max)
        {
            mSWTotalObjects->setText(getString("objects_deleted_text"));
            mSWTotalObjects->setTextArg("[DELETED]", llformat("%d", sw_total - sw_max));
        }
        else
        {
            mSWTotalObjects->setText(getString("objects_available_text"));
            mSWTotalObjects->setTextArg("[AVAILABLE]", llformat("%d", sw_max - sw_total));
        }
        mSWTotalObjects->setTextArg("[COUNT]", llformat("%d", sw_total));
        mSWTotalObjects->setTextArg("[MAX]", llformat("%d", sw_max));

        mObjectContribution->setTextArg("[COUNT]", llformat("%d", max));
        mTotalObjects->setTextArg("[COUNT]", llformat("%d", total));
        mOwnerObjects->setTextArg("[COUNT]", llformat("%d", owned));
        mGroupObjects->setTextArg("[COUNT]", llformat("%d", group));
        mOtherObjects->setTextArg("[COUNT]", llformat("%d", other));
        mSelectedObjects->setTextArg("[COUNT]", llformat("%d", selected));
        mCleanOtherObjectsTime->setText(llformat("%d", mOtherTime));

        bool can_return_owned = LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_LAND_RETURN_GROUP_OWNED);
        bool can_return_group_set = LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_LAND_RETURN_GROUP_SET);
        bool can_return_other = LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_LAND_RETURN_NON_GROUP);

        if (can_return_owned || can_return_group_set || can_return_other)
        {
            if (owned && can_return_owned)
            {
                mBtnShowOwnerObjects->setEnabled(true);
                mBtnReturnOwnerObjects->setEnabled(true);
            }
            if (group && can_return_group_set)
            {
                mBtnShowGroupObjects->setEnabled(true);
                mBtnReturnGroupObjects->setEnabled(true);
            }
            if (other && can_return_other)
            {
                mBtnShowOtherObjects->setEnabled(true);
                mBtnReturnOtherObjects->setEnabled(true);
            }

            mCleanOtherObjectsTime->setEnabled(true);
            mBtnRefresh->setEnabled(true);
        }
    }
}

// virtual
void LLPanelLandObjects::draw()
{
    LLPanel::draw();
}

void send_other_clean_time_message(S32 parcel_local_id, S32 other_clean_time)
{
    LLMessageSystem *msg = gMessageSystem;

    LLViewerRegion* region = LLViewerParcelMgr::getInstance()->getSelectionRegion();
    if (!region) return;

    msg->newMessageFast(_PREHASH_ParcelSetOtherCleanTime);
    msg->nextBlockFast(_PREHASH_AgentData);
    msg->addUUIDFast(_PREHASH_AgentID,  gAgent.getID());
    msg->addUUIDFast(_PREHASH_SessionID,gAgent.getSessionID());
    msg->nextBlockFast(_PREHASH_ParcelData);
    msg->addS32Fast(_PREHASH_LocalID, parcel_local_id);
    msg->addS32Fast(_PREHASH_OtherCleanTime, other_clean_time);

    msg->sendReliable(region->getHost());
}

void send_return_objects_message(S32 parcel_local_id, S32 return_type,
                                 uuid_list_t* owner_ids = NULL)
{
    LLMessageSystem *msg = gMessageSystem;

    LLViewerRegion* region = LLViewerParcelMgr::getInstance()->getSelectionRegion();
    if (!region) return;

    msg->newMessageFast(_PREHASH_ParcelReturnObjects);
    msg->nextBlockFast(_PREHASH_AgentData);
    msg->addUUIDFast(_PREHASH_AgentID,  gAgent.getID());
    msg->addUUIDFast(_PREHASH_SessionID,gAgent.getSessionID());
    msg->nextBlockFast(_PREHASH_ParcelData);
    msg->addS32Fast(_PREHASH_LocalID, parcel_local_id);
    msg->addU32Fast(_PREHASH_ReturnType, (U32) return_type);

    // Dummy task id, not used
    msg->nextBlock("TaskIDs");
    msg->addUUID("TaskID", LLUUID::null);

    // Throw all return ids into the packet.
    // TODO: Check for too many ids.
    if (owner_ids)
    {
        uuid_list_t::iterator end = owner_ids->end();
        for (uuid_list_t::iterator it = owner_ids->begin();
             it != end;
             ++it)
        {
            msg->nextBlockFast(_PREHASH_OwnerIDs);
            msg->addUUIDFast(_PREHASH_OwnerID, (*it));
        }
    }
    else
    {
        msg->nextBlockFast(_PREHASH_OwnerIDs);
        msg->addUUIDFast(_PREHASH_OwnerID, LLUUID::null);
    }

    msg->sendReliable(region->getHost());
}

bool LLPanelLandObjects::callbackReturnOwnerObjects(const LLSD& notification, const LLSD& response)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    LLParcel *parcel = mParcel->getParcel();
    if (0 == option)
    {
        if (parcel)
        {
            LLUUID owner_id = parcel->getOwnerID();
            LLSD args;
            if (owner_id == gAgentID)
            {
                LLNotificationsUtil::add("OwnedObjectsReturned");
            }
            else
            {
                args["NAME"] = LLSLURL("agent", owner_id, "completename").getSLURLString();
                LLNotificationsUtil::add("OtherObjectsReturned", args);
            }
            send_return_objects_message(parcel->getLocalID(), RT_OWNER);
        }
    }

    LLSelectMgr::getInstance()->unhighlightAll();
    LLViewerParcelMgr::getInstance()->sendParcelPropertiesUpdate( parcel );
    refresh();
    return false;
}

bool LLPanelLandObjects::callbackReturnGroupObjects(const LLSD& notification, const LLSD& response)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    LLParcel *parcel = mParcel->getParcel();
    if (0 == option)
    {
        if (parcel)
        {
            std::string group_name;
            gCacheName->getGroupName(parcel->getGroupID(), group_name);
            LLSD args;
            args["GROUPNAME"] = group_name;
            LLNotificationsUtil::add("GroupObjectsReturned", args);
            send_return_objects_message(parcel->getLocalID(), RT_GROUP);
        }
    }
    LLSelectMgr::getInstance()->unhighlightAll();
    LLViewerParcelMgr::getInstance()->sendParcelPropertiesUpdate( parcel );
    refresh();
    return false;
}

bool LLPanelLandObjects::callbackReturnOtherObjects(const LLSD& notification, const LLSD& response)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    LLParcel *parcel = mParcel->getParcel();
    if (0 == option)
    {
        if (parcel)
        {
            LLNotificationsUtil::add("UnOwnedObjectsReturned");
            send_return_objects_message(parcel->getLocalID(), RT_OTHER);
        }
    }
    LLSelectMgr::getInstance()->unhighlightAll();
    LLViewerParcelMgr::getInstance()->sendParcelPropertiesUpdate( parcel );
    refresh();
    return false;
}

bool LLPanelLandObjects::callbackReturnOwnerList(const LLSD& notification, const LLSD& response)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    LLParcel *parcel = mParcel->getParcel();
    if (0 == option)
    {
        if (parcel)
        {
            // Make sure we have something selected.
            uuid_list_t::iterator selected = mSelectedOwners.begin();
            if (selected != mSelectedOwners.end())
            {
                LLSD args;
                if (mSelectedIsGroup)
                {
                    args["GROUPNAME"] = mSelectedName;
                    LLNotificationsUtil::add("GroupObjectsReturned", args);
                }
                else
                {
                    args["NAME"] = mSelectedName;
                    LLNotificationsUtil::add("OtherObjectsReturned2", args);
                }

                send_return_objects_message(parcel->getLocalID(), RT_LIST, &(mSelectedOwners));
            }
        }
    }
    LLSelectMgr::getInstance()->unhighlightAll();
    LLViewerParcelMgr::getInstance()->sendParcelPropertiesUpdate( parcel );
    refresh();
    return false;
}


// static
void LLPanelLandObjects::onClickReturnOwnerList(void* userdata)
{
    LLPanelLandObjects  *self = (LLPanelLandObjects *)userdata;

    LLParcel* parcelp = self->mParcel->getParcel();
    if (!parcelp) return;

    // Make sure we have something selected.
    if (self->mSelectedOwners.empty())
    {
        return;
    }
    //uuid_list_t::iterator selected_itr = self->mSelectedOwners.begin();
    //if (selected_itr == self->mSelectedOwners.end()) return;

    send_parcel_select_objects(parcelp->getLocalID(), RT_LIST, &(self->mSelectedOwners));

    LLSD args;
    args["NAME"] = self->mSelectedName;
    args["N"] = llformat("%d",self->mSelectedCount);
    if (self->mSelectedIsGroup)
    {
        LLNotificationsUtil::add("ReturnObjectsDeededToGroup", args, LLSD(), boost::bind(&LLPanelLandObjects::callbackReturnOwnerList, self, _1, _2));
    }
    else
    {
        LLNotificationsUtil::add("ReturnObjectsOwnedByUser", args, LLSD(), boost::bind(&LLPanelLandObjects::callbackReturnOwnerList, self, _1, _2));
    }
}


// static
void LLPanelLandObjects::onClickRefresh(void* userdata)
{
    LLPanelLandObjects *self = (LLPanelLandObjects*)userdata;

    LLMessageSystem *msg = gMessageSystem;

    LLParcel* parcel = self->mParcel->getParcel();
    if (!parcel) return;

    LLViewerRegion* region = LLViewerParcelMgr::getInstance()->getSelectionRegion();
    if (!region) return;

    self->mBtnRefresh->setEnabled(false);

    // ready the list for results
    self->mOwnerList->deleteAllItems();
    self->mOwnerList->setCommentText(LLTrans::getString("Searching"));
    self->mOwnerList->setEnabled(false);
    self->mFirstReply = true;

    // send the message
    msg->newMessageFast(_PREHASH_ParcelObjectOwnersRequest);
    msg->nextBlockFast(_PREHASH_AgentData);
    msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
    msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
    msg->nextBlockFast(_PREHASH_ParcelData);
    msg->addS32Fast(_PREHASH_LocalID, parcel->getLocalID());

    msg->sendReliable(region->getHost());
}

// static
void LLPanelLandObjects::processParcelObjectOwnersReply(LLMessageSystem *msg, void **)
{
    LLPanelLandObjects* self = LLFloaterLand::getCurrentPanelLandObjects();

    if (!self)
    {
        LL_WARNS() << "Received message for nonexistent LLPanelLandObject"
                << LL_ENDL;
        return;
    }

    const LLFontGL* FONT = LLFontGL::getFontSansSerif();

    // Extract all of the owners.
    S32 rows = msg->getNumberOfBlocksFast(_PREHASH_Data);
    //uuid_list_t return_ids;
    LLUUID  owner_id;
    bool    is_group_owned;
    S32     object_count;
    U32     most_recent_time = 0;
    bool    is_online;
    std::string object_count_str;
    //bool b_need_refresh = false;

    // If we were waiting for the first reply, clear the "Searching..." text.
    if (self->mFirstReply)
    {
        self->mOwnerList->deleteAllItems();
        self->mFirstReply = false;
    }

    // <FS:Ansariel> FIRE-1292: Highlight avatars in same region; Online status in
    //               ParcelObjectOwnersReply message was intentionally deprecated by LL!
    std::vector<LLVector3d> positions;
    std::vector<LLUUID> avatar_ids;
    LLUUID own_region_id;

    LLViewerRegion* own_region = gAgent.getRegion();
    if (own_region)
    {
        own_region_id = own_region->getRegionID();
    }

    LLWorld::getInstance()->getAvatars(&avatar_ids, &positions, gAgent.getPositionGlobal(), 8192.f);
    // </FS:Ansariel>

    for(S32 i = 0; i < rows; ++i)
    {
        msg->getUUIDFast(_PREHASH_Data, _PREHASH_OwnerID,       owner_id,       i);
        msg->getBOOLFast(_PREHASH_Data, _PREHASH_IsGroupOwned,  is_group_owned, i);
        msg->getS32Fast (_PREHASH_Data, _PREHASH_Count,         object_count,   i);
        msg->getBOOLFast(_PREHASH_Data, _PREHASH_OnlineStatus,  is_online,      i);
        if(msg->has("DataExtended"))
        {
            msg->getU32("DataExtended", "TimeStamp", most_recent_time, i);
        }

        if (owner_id.isNull())
        {
            continue;
        }

        // <FS:Ansariel> FIRE-1292: Highlight avatars in same region; Online status in
        //               ParcelObjectOwnersReply message was intentionally deprecated by LL!
        if (gAgentID == owner_id)
        {
            is_online = true;
        }
        else
        {
            is_online = false;
            for (U32 i = 0; i < avatar_ids.size(); i++)
            {
                if (avatar_ids[i] == owner_id)
                {
                    LLViewerRegion* avatar_region = LLWorld::getInstance()->getRegionFromPosGlobal(positions[i]);
                    if (avatar_region && avatar_region->getRegionID() == own_region_id)
                    {
                        is_online = true;
                    }
                    break;
                }
            }
        }
        // </FS:Ansariel>

        LLNameListCtrl::NameItem item_params;
        item_params.value = owner_id;
        item_params.target = is_group_owned ? LLNameListCtrl::GROUP : LLNameListCtrl::INDIVIDUAL;

        if (is_group_owned)
        {
            item_params.columns.add().type("icon").value(self->mIconGroup->getName()).column("type");
            item_params.columns.add().value(OWNER_GROUP).font(FONT).column("online_status");
        }
        else if (is_online)
        {
            item_params.columns.add().type("icon").value(self->mIconAvatarOnline->getName()).column("type");
            item_params.columns.add().value(OWNER_ONLINE).font(FONT).column("online_status");
        }
        else  // offline
        {
            item_params.columns.add().type("icon").value(self->mIconAvatarOffline->getName()).column("type");
            item_params.columns.add().value(OWNER_OFFLINE).font(FONT).column("online_status");
        }

        // Placeholder for name.
        LLAvatarName av_name;
        // <FS:Ansariel> The name list will do that for us - this implementation doesn't work anyway!
        //LLAvatarNameCache::get(owner_id, &av_name);
        //item_params.columns.add().value(av_name.getCompleteName()).font(FONT).column("name");
        item_params.columns.add().font(FONT).column("name");
        item_params.name = LLTrans::getString("AvatarNameWaiting");
        // </FS:Ansariel>

        object_count_str = llformat("%d", object_count);
        item_params.columns.add().value(object_count_str).font(FONT).column("count");
        item_params.columns.add().value(LLDate((double)most_recent_time)).font(FONT).column("mostrecent").type("date");

        self->mOwnerList->addNameItemRow(item_params);
        LL_DEBUGS() << "object owner " << owner_id << " (" << (is_group_owned ? "group" : "agent")
                << ") owns " << object_count << " objects." << LL_ENDL;
    }

    // check for no results
    if (0 == self->mOwnerList->getItemCount())
    {
        self->mOwnerList->setCommentText(LLTrans::getString("NoneFound"));
    }
    else
    {
        self->mOwnerList->setEnabled(true);
    }

    self->mBtnRefresh->setEnabled(true);
}

// static
void LLPanelLandObjects::onCommitList(LLUICtrl* ctrl, void* data)
{
    LLPanelLandObjects* self = (LLPanelLandObjects*)data;

    if (false == self->mOwnerList->getCanSelect())
    {
        return;
    }
    LLScrollListItem *item = self->mOwnerList->getFirstSelected();
    if (item)
    {
        // Look up the selected name, for future dialog box use.
        const LLScrollListCell* cell;
        cell = item->getColumn(1);
        if (!cell)
        {
            return;
        }
        // Is this a group?
        self->mSelectedIsGroup = cell->getValue().asString() == OWNER_GROUP;
        cell = item->getColumn(2);
        self->mSelectedName = cell->getValue().asString();
        cell = item->getColumn(3);
        self->mSelectedCount = atoi(cell->getValue().asString().c_str());

        // Set the selection, and enable the return button.
        self->mSelectedOwners.clear();
        self->mSelectedOwners.insert(item->getUUID());
        self->mBtnReturnOwnerList->setEnabled(true);

        // Highlight this user's objects
        clickShowCore(self, RT_LIST, &(self->mSelectedOwners));
    }
}

// static
void LLPanelLandObjects::clickShowCore(LLPanelLandObjects* self, S32 return_type, uuid_list_t* list)
{
    LLParcel* parcel = self->mParcel->getParcel();
    if (!parcel) return;

    send_parcel_select_objects(parcel->getLocalID(), return_type, list);
}

// static
void LLPanelLandObjects::onClickShowOwnerObjects(void* userdata)
{
    clickShowCore((LLPanelLandObjects*)userdata, RT_OWNER);
}

// static
void LLPanelLandObjects::onClickShowGroupObjects(void* userdata)
{
    clickShowCore((LLPanelLandObjects*)userdata, (RT_GROUP));
}

// static
void LLPanelLandObjects::onClickShowOtherObjects(void* userdata)
{
    clickShowCore((LLPanelLandObjects*)userdata, RT_OTHER);
}

// static
void LLPanelLandObjects::onClickReturnOwnerObjects(void* userdata)
{
    S32 owned = 0;

    LLPanelLandObjects* panelp = (LLPanelLandObjects*)userdata;
    LLParcel* parcel = panelp->mParcel->getParcel();
    if (!parcel) return;

    owned = parcel->getOwnerPrimCount();

    send_parcel_select_objects(parcel->getLocalID(), RT_OWNER);

    LLUUID owner_id = parcel->getOwnerID();

    LLSD args;
    args["N"] = llformat("%d",owned);

    if (owner_id == gAgent.getID())
    {
        LLNotificationsUtil::add("ReturnObjectsOwnedBySelf", args, LLSD(), boost::bind(&LLPanelLandObjects::callbackReturnOwnerObjects, panelp, _1, _2));
    }
    else
    {
        args["NAME"] = LLSLURL("agent", owner_id, "completename").getSLURLString();
        LLNotificationsUtil::add("ReturnObjectsOwnedByUser", args, LLSD(), boost::bind(&LLPanelLandObjects::callbackReturnOwnerObjects, panelp, _1, _2));
    }
}

// static
void LLPanelLandObjects::onClickReturnGroupObjects(void* userdata)
{
    LLPanelLandObjects* panelp = (LLPanelLandObjects*)userdata;
    LLParcel* parcel = panelp->mParcel->getParcel();
    if (!parcel) return;

    send_parcel_select_objects(parcel->getLocalID(), RT_GROUP);

    std::string group_name;
    gCacheName->getGroupName(parcel->getGroupID(), group_name);

    LLSD args;
    args["NAME"] = group_name;
    args["N"] = llformat("%d", parcel->getGroupPrimCount());

    // create and show confirmation textbox
    LLNotificationsUtil::add("ReturnObjectsDeededToGroup", args, LLSD(), boost::bind(&LLPanelLandObjects::callbackReturnGroupObjects, panelp, _1, _2));
}

// static
void LLPanelLandObjects::onClickReturnOtherObjects(void* userdata)
{
    S32 other = 0;

    LLPanelLandObjects* panelp = (LLPanelLandObjects*)userdata;
    LLParcel* parcel = panelp->mParcel->getParcel();
    if (!parcel) return;

    other = parcel->getOtherPrimCount();

    send_parcel_select_objects(parcel->getLocalID(), RT_OTHER);

    LLSD args;
    args["N"] = llformat("%d", other);

    if (parcel->getIsGroupOwned())
    {
        std::string group_name;
        gCacheName->getGroupName(parcel->getGroupID(), group_name);
        args["NAME"] = group_name;

        LLNotificationsUtil::add("ReturnObjectsNotOwnedByGroup", args, LLSD(), boost::bind(&LLPanelLandObjects::callbackReturnOtherObjects, panelp, _1, _2));
    }
    else
    {
        LLUUID owner_id = parcel->getOwnerID();

        if (owner_id == gAgent.getID())
        {
            LLNotificationsUtil::add("ReturnObjectsNotOwnedBySelf", args, LLSD(), boost::bind(&LLPanelLandObjects::callbackReturnOtherObjects, panelp, _1, _2));
        }
        else
        {
            args["NAME"] = LLSLURL("agent", owner_id, "completename").getSLURLString();
            LLNotificationsUtil::add("ReturnObjectsNotOwnedByUser", args, LLSD(), boost::bind(&LLPanelLandObjects::callbackReturnOtherObjects, panelp, _1, _2));
        }
    }
}

// static
void LLPanelLandObjects::onLostFocus(LLFocusableElement* caller, void* user_data)
{
    onCommitClean((LLUICtrl*)caller, user_data);
}

// static
void LLPanelLandObjects::onCommitClean(LLUICtrl *caller, void* user_data)
{
    LLPanelLandObjects  *lop = (LLPanelLandObjects *)user_data;
    LLParcel* parcel = lop->mParcel->getParcel();
    if (parcel)
    {
        S32 return_time = atoi(lop->mCleanOtherObjectsTime->getText().c_str());
        // Only send return time if it has changed
        if (return_time != lop->mOtherTime)
        {
            lop->mOtherTime = return_time;

        parcel->setCleanOtherTime(lop->mOtherTime);
        send_other_clean_time_message(parcel->getLocalID(), lop->mOtherTime);
    }
}
}


//---------------------------------------------------------------------------
// LLPanelLandOptions
//---------------------------------------------------------------------------

LLPanelLandOptions::LLPanelLandOptions(LLParcelSelectionHandle& parcel)
:   LLPanel(),
    mCheckEditObjects(NULL),
    mCheckEditGroupObjects(NULL),
    mCheckAllObjectEntry(NULL),
    mCheckGroupObjectEntry(NULL),
    mCheckEditLand(NULL), // <FS:WF> FIRE-6604 : Reinstate the "Allow Other Residents to Edit Terrain" option in About Land
    mCheckSafe(NULL),
    mCheckFly(NULL),
    mCheckGroupScripts(NULL),
    mCheckOtherScripts(NULL),
    mCheckShowDirectory(NULL),
    mCategoryCombo(NULL),
    mLandingTypeCombo(NULL),
    mSnapshotCtrl(NULL),
    mLocationText(NULL),
    mSeeAvatarsText(NULL),
    mSetBtn(NULL),
    mClearBtn(NULL),
    mMatureCtrl(NULL),
    mPushRestrictionCtrl(NULL),
    mSeeAvatarsCtrl(NULL),
    mParcel(parcel)
{
}


bool LLPanelLandOptions::postBuild()
{
    mCheckEditObjects = getChild<LLCheckBoxCtrl>( "edit objects check");
    childSetCommitCallback("edit objects check", onCommitAny, this);

    mCheckEditGroupObjects = getChild<LLCheckBoxCtrl>( "edit group objects check");
    childSetCommitCallback("edit group objects check", onCommitAny, this);

    mCheckAllObjectEntry = getChild<LLCheckBoxCtrl>( "all object entry check");
    childSetCommitCallback("all object entry check", onCommitAny, this);

    mCheckGroupObjectEntry = getChild<LLCheckBoxCtrl>( "group object entry check");
    childSetCommitCallback("group object entry check", onCommitAny, this);

  // <FS:WF> FIRE-6604 : Reinstate the "Allow Other Residents to Edit Terrain" option in About Land
    mCheckEditLand = getChild<LLCheckBoxCtrl>( "edit land check");
    childSetCommitCallback("edit land check", onCommitAny, this);
  // <FS:WF>

    mCheckGroupScripts = getChild<LLCheckBoxCtrl>( "check group scripts");
    childSetCommitCallback("check group scripts", onCommitAny, this);


    mCheckFly = getChild<LLCheckBoxCtrl>( "check fly");
    childSetCommitCallback("check fly", onCommitAny, this);


    mCheckOtherScripts = getChild<LLCheckBoxCtrl>( "check other scripts");
    childSetCommitCallback("check other scripts", onCommitAny, this);


    mCheckSafe = getChild<LLCheckBoxCtrl>( "check safe");
    childSetCommitCallback("check safe", onCommitAny, this);


    mPushRestrictionCtrl = getChild<LLCheckBoxCtrl>( "PushRestrictCheck");
    childSetCommitCallback("PushRestrictCheck", onCommitAny, this);

    mSeeAvatarsCtrl = getChild<LLCheckBoxCtrl>( "SeeAvatarsCheck");
    childSetCommitCallback("SeeAvatarsCheck", onCommitAny, this);

    mSeeAvatarsText = getChild<LLTextBox>("allow_see_label");
    if (mSeeAvatarsText)
    {
        mSeeAvatarsText->setShowCursorHand(false);
        mSeeAvatarsText->setSoundFlags(LLView::MOUSE_UP);
        mSeeAvatarsText->setClickedCallback(boost::bind(&toggleSeeAvatars, this));
    }

    mCheckShowDirectory = getChild<LLCheckBoxCtrl>( "ShowDirectoryCheck");
    childSetCommitCallback("ShowDirectoryCheck", onCommitAny, this);


    mCategoryCombo = getChild<LLComboBox>( "land category");
    childSetCommitCallback("land category", onCommitAny, this);


    mMatureCtrl = getChild<LLCheckBoxCtrl>( "MatureCheck");
    childSetCommitCallback("MatureCheck", onCommitAny, this);

    if (gAgent.wantsPGOnly())
    {
        // Disable these buttons if they are PG (Teen) users
        mMatureCtrl->setVisible(false);
        mMatureCtrl->setEnabled(false);
    }


    mSnapshotCtrl = getChild<LLTextureCtrl>("snapshot_ctrl");
    if (mSnapshotCtrl)
    {
        mSnapshotCtrl->setCommitCallback( onCommitAny, this );
        mSnapshotCtrl->setAllowNoTexture ( true );
        mSnapshotCtrl->setImmediateFilterPermMask(PERM_COPY | PERM_TRANSFER);
        mSnapshotCtrl->setDnDFilterPermMask(PERM_COPY | PERM_TRANSFER);
    }
    else
    {
        LL_WARNS() << "LLUICtrlFactory::getTexturePickerByName() returned NULL for 'snapshot_ctrl'" << LL_ENDL;
    }


    mLocationText = getChild<LLTextBox>("landing_point");

    mSetBtn = getChild<LLButton>("Set");
    mSetBtn->setClickedCallback(onClickSet, this);


    mClearBtn = getChild<LLButton>("Clear");
    mClearBtn->setClickedCallback(onClickClear, this);


    mLandingTypeCombo = getChild<LLComboBox>( "landing type");
    childSetCommitCallback("landing type", onCommitAny, this);

    // <FS:Ansariel> FIRE-10043: Teleport to LP button
    mTeleportToLandingPointBtn = getChild<LLButton>("teleport_to_landing_point");
    mTeleportToLandingPointBtn->setCommitCallback(boost::bind(&LLPanelLandOptions::onClickTeleport, this));
    // </FS:Ansariel>

    return true;
}


// virtual
LLPanelLandOptions::~LLPanelLandOptions()
{ }


// virtual
void LLPanelLandOptions::refresh()
{
    refreshSearch();

    LLParcel *parcel = mParcel->getParcel();
    if (!parcel || gDisconnected)
    {
        mCheckEditObjects   ->set(false);
        mCheckEditObjects   ->setEnabled(false);

        mCheckEditGroupObjects  ->set(false);
        mCheckEditGroupObjects  ->setEnabled(false);

        mCheckAllObjectEntry    ->set(false);
        mCheckAllObjectEntry    ->setEnabled(false);

        mCheckGroupObjectEntry  ->set(false);
        mCheckGroupObjectEntry  ->setEnabled(false);

     // <FS:WF> FIRE-6604 : Reinstate the "Allow Other Residents to Edit Terrain" option in About Land
       if ( mCheckEditLand )
       {
            mCheckEditLand      ->set(false);
            mCheckEditLand      ->setEnabled(false);
       }
     // <FS:WF>

        mCheckSafe          ->set(false);
        mCheckSafe          ->setEnabled(false);

        mCheckFly           ->set(false);
        mCheckFly           ->setEnabled(false);

        mCheckGroupScripts  ->set(false);
        mCheckGroupScripts  ->setEnabled(false);

        mCheckOtherScripts  ->set(false);
        mCheckOtherScripts  ->setEnabled(false);

        mPushRestrictionCtrl->set(false);
        mPushRestrictionCtrl->setEnabled(false);

        mSeeAvatarsCtrl->set(true);
        mSeeAvatarsCtrl->setEnabled(false);
        mSeeAvatarsText->setEnabled(false);

        mLandingTypeCombo->setCurrentByIndex(0);
        mLandingTypeCombo->setEnabled(false);

        mSnapshotCtrl->setImageAssetID(LLUUID::null);
        mSnapshotCtrl->setEnabled(false);

        mLocationText->setTextArg("[LANDING]", getString("landing_point_none"));
        mSetBtn->setEnabled(false);
        mClearBtn->setEnabled(false);

        mMatureCtrl->setEnabled(false);

        // <FS:Ansariel> FIRE-10043: Teleport to LP button
        mTeleportToLandingPointBtn->setEnabled(false);
        // </FS:Ansariel>
    }
    else
    {
        // something selected, hooray!

        // Display options
        bool can_change_options = LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_LAND_OPTIONS);
        mCheckEditObjects   ->set( parcel->getAllowModify() );
        mCheckEditObjects   ->setEnabled( can_change_options );

        mCheckEditGroupObjects  ->set( parcel->getAllowGroupModify() ||  parcel->getAllowModify());
        mCheckEditGroupObjects  ->setEnabled( can_change_options && !parcel->getAllowModify() );        // If others edit is enabled, then this is explicitly enabled.

        mCheckAllObjectEntry    ->set( parcel->getAllowAllObjectEntry() );
        mCheckAllObjectEntry    ->setEnabled( can_change_options );

        mCheckGroupObjectEntry  ->set( parcel->getAllowGroupObjectEntry() ||  parcel->getAllowAllObjectEntry());
        mCheckGroupObjectEntry  ->setEnabled( can_change_options && !parcel->getAllowAllObjectEntry() );


        // <FS:WF> FIRE-6604 : Reinstate the "Allow Other Residents to Edit Terrain" option in About Land
        bool can_change_terraform = LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_LAND_EDIT);
        mCheckEditLand      ->set( parcel->getAllowTerraform() );
        mCheckEditLand      ->setEnabled( can_change_terraform );
        // <FS:WF>

        mCheckSafe          ->set( !parcel->getAllowDamage() );
        mCheckSafe          ->setEnabled( can_change_options );

        mCheckFly           ->set( parcel->getAllowFly() );
        mCheckFly           ->setEnabled( can_change_options );

        mCheckGroupScripts  ->set( parcel->getAllowGroupScripts() || parcel->getAllowOtherScripts());
        mCheckGroupScripts  ->setEnabled( can_change_options && !parcel->getAllowOtherScripts());

        mCheckOtherScripts  ->set( parcel->getAllowOtherScripts() );
        mCheckOtherScripts  ->setEnabled( can_change_options );

        mPushRestrictionCtrl->set( parcel->getRestrictPushObject() );
        if(parcel->getRegionPushOverride())
        {
            mPushRestrictionCtrl->setLabel(getString("push_restrict_region_text"));
            mPushRestrictionCtrl->setEnabled(false);
            mPushRestrictionCtrl->set(true);
        }
        else
        {
            mPushRestrictionCtrl->setLabel(getString("push_restrict_text"));
            mPushRestrictionCtrl->setEnabled(can_change_options);
        }

        mSeeAvatarsCtrl->set(parcel->getSeeAVs());
        mSeeAvatarsCtrl->setEnabled(can_change_options && parcel->getHaveNewParcelLimitData());
        mSeeAvatarsText->setEnabled(can_change_options && parcel->getHaveNewParcelLimitData());

        bool can_change_landing_point = LLViewerParcelMgr::isParcelModifiableByAgent(parcel,
                                                        GP_LAND_SET_LANDING_POINT);
        mLandingTypeCombo->setCurrentByIndex((S32)parcel->getLandingType());
        mLandingTypeCombo->setEnabled( can_change_landing_point );

        bool can_change_identity =
                LLViewerParcelMgr::isParcelModifiableByAgent(
                    parcel, GP_LAND_CHANGE_IDENTITY);
        mSnapshotCtrl->setImageAssetID(parcel->getSnapshotID());
        mSnapshotCtrl->setEnabled( can_change_identity );

        // find out where we're looking and convert that to an angle in degrees on a regular compass (not the internal representation)
        LLVector3 user_look_at = parcel->getUserLookAt();
        U32 user_look_at_angle = ( (U32)( ( atan2(user_look_at[1], -user_look_at[0]) + F_PI * 2 ) * RAD_TO_DEG + 0.5) - 90) % 360;

        LLVector3 pos = parcel->getUserLocation();
        if (pos.isExactlyZero())
        {
            mLocationText->setTextArg("[LANDING]", getString("landing_point_none"));
            // <FS:Ansariel> FIRE-10043: Teleport to LP button
            mTeleportToLandingPointBtn->setEnabled(false);
            // </FS:Ansariel>
        }
        else
        {
            mLocationText->setTextArg("[LANDING]",llformat("%d, %d, %d (%d\xC2\xB0)",
                                                           ll_round(pos.mV[VX]),
                                                           ll_round(pos.mV[VY]),
                                                           ll_round(pos.mV[VZ]),
                                                           user_look_at_angle));
            // <FS:Ansariel> FIRE-10043: Teleport to LP button
            mTeleportToLandingPointBtn->setEnabled(true);
            // </FS:Ansariel>
        }

        mSetBtn->setEnabled( can_change_landing_point );
        mClearBtn->setEnabled( can_change_landing_point );

        if (gAgent.wantsPGOnly())
        {
            // Disable these buttons if they are PG (Teen) users
            mMatureCtrl->setVisible(false);
            mMatureCtrl->setEnabled(false);
        }
        else
        {
            // not teen so fill in the data for the maturity control
            mMatureCtrl->setVisible(true);
            LLStyle::Params style;
            style.image(LLUI::getUIImage(gFloaterView->getParentFloater(this)->getString("maturity_icon_moderate")));
            LLCheckBoxWithTBAcess* fullaccess_mature_ctrl = (LLCheckBoxWithTBAcess*)mMatureCtrl;
            fullaccess_mature_ctrl->getTextBox()->setText(LLStringExplicit(""));
            fullaccess_mature_ctrl->getTextBox()->appendImageSegment(style);
            fullaccess_mature_ctrl->getTextBox()->appendText(getString("mature_check_mature"), false);
            fullaccess_mature_ctrl->setToolTip(getString("mature_check_mature_tooltip"));
            fullaccess_mature_ctrl->reshape(fullaccess_mature_ctrl->getRect().getWidth(), fullaccess_mature_ctrl->getRect().getHeight(), false);

            // they can see the checkbox, but its disposition depends on the
            // state of the region
            LLViewerRegion* regionp = LLViewerParcelMgr::getInstance()->getSelectionRegion();
            if (regionp)
            {
                if (regionp->getSimAccess() == SIM_ACCESS_PG)
                {
                    mMatureCtrl->setEnabled(false);
                    mMatureCtrl->set(false);
                }
                else if (regionp->getSimAccess() == SIM_ACCESS_MATURE)
                {
                    mMatureCtrl->setEnabled(can_change_identity);
                    mMatureCtrl->set(parcel->getMaturePublish());
                }
                else if (regionp->getSimAccess() == SIM_ACCESS_ADULT)
                {
                    mMatureCtrl->setEnabled(false);
                    mMatureCtrl->set(true);
                    mMatureCtrl->setLabel(getString("mature_check_adult"));
                    mMatureCtrl->setToolTip(getString("mature_check_adult_tooltip"));
                }
            }
        }
        S32 fee = getDirectoryFee();
        if (fee == 0)
        {
            mCheckShowDirectory->setLabel(getString("DirectoryFree"));
        }
        else
        {
            LLStringUtil::format_map_t map;
            map["DIRECTORY_FEE"] = llformat("%d", fee);
            mCheckShowDirectory->setLabel(getString("DirectoryFee", map));
        }
    }
}

S32 LLPanelLandOptions::getDirectoryFee()
{
    S32 fee = PARCEL_DIRECTORY_FEE;
#ifdef OPENSIM
    if (LLGridManager::getInstance()->isInOpenSim())
    {
        fee = LLGridManager::getInstance()->getDirectoryFee();
    }
    if (LLGridManager::getInstance()->isInAuroraSim())
    {
        LLSD grid_info;
        LLGridManager::getInstance()->getGridData(grid_info);
        fee = grid_info[GRID_DIRECTORY_FEE].asInteger();
    }
#endif // OPENSIM
    return fee;
}

// virtual
void LLPanelLandOptions::draw()
{
    LLPanel::draw();
}


// private
void LLPanelLandOptions::refreshSearch()
{
    LLParcel *parcel = mParcel->getParcel();
    if (!parcel || gDisconnected)
    {
        mCheckShowDirectory->set(false);
        mCheckShowDirectory->setEnabled(false);

        const std::string& none_string = LLParcel::getCategoryString(LLParcel::C_NONE);
        mCategoryCombo->setValue(none_string);
        mCategoryCombo->setEnabled(false);
        return;
    }

    LLViewerRegion* region =
            LLViewerParcelMgr::getInstance()->getSelectionRegion();

    bool can_change =
            LLViewerParcelMgr::isParcelModifiableByAgent(
                parcel, GP_LAND_FIND_PLACES)
            && region
            && !(region->getRegionFlag(REGION_FLAGS_BLOCK_PARCEL_SEARCH));

    bool show_directory = parcel->getParcelFlag(PF_SHOW_DIRECTORY);
    mCheckShowDirectory->set(show_directory);

    // Set by string in case the order in UI doesn't match the order by index.
    LLParcel::ECategory cat = parcel->getCategory();
    const std::string& category_string = LLParcel::getCategoryString(cat);
    mCategoryCombo->setValue(category_string);

    std::string tooltip;
    bool enable_show_directory = false;
    // Parcels <= 128 square meters cannot be listed in search, in an
    // effort to reduce search spam from small parcels.  See also
    // the search crawler "grid-crawl.py" in secondlife.com/doc/app/search/ JC
    const S32 MIN_PARCEL_AREA_FOR_SEARCH = 128;
    bool large_enough = parcel->getArea() >= MIN_PARCEL_AREA_FOR_SEARCH;
    if (large_enough)
    {
        if (can_change)
        {
            tooltip = getString("search_enabled_tooltip");
            enable_show_directory = true;
        }
        else
        {
            tooltip = getString("search_disabled_permissions_tooltip");
            enable_show_directory = false;
        }
    }
    else
    {
        // not large enough to include in search
        if (can_change)
        {
            if (show_directory)
            {
                // parcels that are too small, but are still in search for
                // legacy reasons, need to have the check box enabled so
                // the owner can delist the parcel. JC
                tooltip = getString("search_enabled_tooltip");
                enable_show_directory = true;
            }
            else
            {
                tooltip = getString("search_disabled_small_tooltip");
                enable_show_directory = false;
            }
        }
        else
        {
            // both too small and don't have permission, so just
            // show the permissions as the reason (which is probably
            // the more common case) JC
            tooltip = getString("search_disabled_permissions_tooltip");
            enable_show_directory = false;
        }
    }
    mCheckShowDirectory->setToolTip(tooltip);
    mCategoryCombo->setToolTip(tooltip);
    mCheckShowDirectory->setEnabled(enable_show_directory);
    mCategoryCombo->setEnabled(enable_show_directory);
}


// static
void LLPanelLandOptions::onCommitAny(LLUICtrl *ctrl, void *userdata)
{
    LLPanelLandOptions *self = (LLPanelLandOptions *)userdata;

    LLParcel* parcel = self->mParcel->getParcel();
    if (!parcel)
    {
        return;
    }

    // Extract data from UI
    bool create_objects     = self->mCheckEditObjects->get();
    bool create_group_objects   = self->mCheckEditGroupObjects->get() || self->mCheckEditObjects->get();
    bool all_object_entry       = self->mCheckAllObjectEntry->get();
    bool group_object_entry = self->mCheckGroupObjectEntry->get() || self->mCheckAllObjectEntry->get();

 // <FS:WF> FIRE-6604 : Reinstate the "Allow Other Residents to Edit Terrain" option in About Land
    bool allow_terraform    = self->mCheckEditLand->get();
 // bool allow_terraform    = false; // removed from UI so always off now - self->mCheckEditLand->get();
 // <FS:WF>
    bool allow_damage       = !self->mCheckSafe->get();
    bool allow_fly          = self->mCheckFly->get();
    bool allow_landmark     = true; // cannot restrict landmark creation
    bool allow_other_scripts    = self->mCheckOtherScripts->get();
    bool allow_group_scripts    = self->mCheckGroupScripts->get() || allow_other_scripts;
    bool allow_publish      = false;
    bool mature_publish     = self->mMatureCtrl->get();
    bool push_restriction   = self->mPushRestrictionCtrl->get();
    bool see_avs            = self->mSeeAvatarsCtrl->get();
    bool show_directory     = self->mCheckShowDirectory->get();
    // we have to get the index from a lookup, not from the position in the dropdown!
    S32  category_index     = LLParcel::getCategoryFromString(self->mCategoryCombo->getSelectedValue());
    S32  landing_type_index = self->mLandingTypeCombo->getCurrentIndex();
    LLUUID snapshot_id      = self->mSnapshotCtrl->getImageAssetID();
    LLViewerRegion* region;
    region = LLViewerParcelMgr::getInstance()->getSelectionRegion();

    if (region && region->getAllowDamage())
    {   // Damage is allowed on the region - server will always allow scripts
        if ( (!allow_other_scripts && parcel->getParcelFlag(PF_ALLOW_OTHER_SCRIPTS)) ||
             (!allow_group_scripts && parcel->getParcelFlag(PF_ALLOW_GROUP_SCRIPTS)) )
        {   // Don't allow turning off "Run Scripts" if damage is allowed in the region
            self->mCheckOtherScripts->set(parcel->getParcelFlag(PF_ALLOW_OTHER_SCRIPTS));   // Restore UI to actual settings
            self->mCheckGroupScripts->set(parcel->getParcelFlag(PF_ALLOW_GROUP_SCRIPTS));
            LLNotificationsUtil::add("UnableToDisableOutsideScripts");
            return;
        }
    }

    // Push data into current parcel
    parcel->setParcelFlag(PF_CREATE_OBJECTS, create_objects);
    parcel->setParcelFlag(PF_CREATE_GROUP_OBJECTS, create_group_objects);
    parcel->setParcelFlag(PF_ALLOW_ALL_OBJECT_ENTRY, all_object_entry);
    parcel->setParcelFlag(PF_ALLOW_GROUP_OBJECT_ENTRY, group_object_entry);
    parcel->setParcelFlag(PF_ALLOW_TERRAFORM, allow_terraform);
    parcel->setParcelFlag(PF_ALLOW_DAMAGE, allow_damage);
    parcel->setParcelFlag(PF_ALLOW_FLY, allow_fly);
    parcel->setParcelFlag(PF_ALLOW_LANDMARK, allow_landmark);
    parcel->setParcelFlag(PF_ALLOW_GROUP_SCRIPTS, allow_group_scripts);
    parcel->setParcelFlag(PF_ALLOW_OTHER_SCRIPTS, allow_other_scripts);
    parcel->setParcelFlag(PF_SHOW_DIRECTORY, show_directory);
    parcel->setParcelFlag(PF_ALLOW_PUBLISH, allow_publish);
    parcel->setParcelFlag(PF_MATURE_PUBLISH, mature_publish);
    parcel->setParcelFlag(PF_RESTRICT_PUSHOBJECT, push_restriction);
    parcel->setCategory((LLParcel::ECategory)category_index);
    parcel->setLandingType((LLParcel::ELandingType)landing_type_index);
    parcel->setSnapshotID(snapshot_id);
    parcel->setSeeAVs(see_avs);

    // Send current parcel data upstream to server
    LLViewerParcelMgr::getInstance()->sendParcelPropertiesUpdate( parcel );

    // Might have changed properties, so let's redraw!
    self->refresh();
}


// static
void LLPanelLandOptions::onClickSet(void* userdata)
{
    LLPanelLandOptions* self = (LLPanelLandOptions*)userdata;

    LLParcel* selected_parcel = self->mParcel->getParcel();
    if (!selected_parcel) return;

    LLParcel* agent_parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
    if (!agent_parcel) return;

    if (agent_parcel->getLocalID() != selected_parcel->getLocalID())
    {
        LLNotificationsUtil::add("MustBeInParcel");
        return;
    }

    LLVector3 pos_region = gAgent.getPositionAgent();
    selected_parcel->setUserLocation(pos_region);
    selected_parcel->setUserLookAt(gAgent.getFrameAgent().getAtAxis());

    LLViewerParcelMgr::getInstance()->sendParcelPropertiesUpdate(selected_parcel);

    self->refresh();
}

void LLPanelLandOptions::onClickClear(void* userdata)
{
    LLPanelLandOptions* self = (LLPanelLandOptions*)userdata;

    LLParcel* selected_parcel = self->mParcel->getParcel();
    if (!selected_parcel) return;

    // yes, this magic number of 0,0,0 means that it is clear
    LLVector3 zero_vec(0.f, 0.f, 0.f);
    selected_parcel->setUserLocation(zero_vec);
    selected_parcel->setUserLookAt(zero_vec);

    LLViewerParcelMgr::getInstance()->sendParcelPropertiesUpdate(selected_parcel);

    self->refresh();
}

void LLPanelLandOptions::toggleSeeAvatars(void* userdata)
{
    LLPanelLandOptions* self = (LLPanelLandOptions*)userdata;
    if (self)
    {
        self->getChild<LLCheckBoxCtrl>("SeeAvatarsCheck")->toggle();
        self->getChild<LLCheckBoxCtrl>("SeeAvatarsCheck")->setBtnFocus();
        self->onCommitAny(NULL, userdata);
    }
}

// <FS:Ansariel> FIRE-10043: Teleport to LP button
void LLPanelLandOptions::onClickTeleport()
{
    LLParcel* selected_parcel = mParcel->getParcel();
    LLViewerRegion* region = LLViewerParcelMgr::getInstance()->getSelectionRegion();
    if (selected_parcel && !selected_parcel->getUserLocation().isExactlyZero() && region)
    {
        gAgent.teleportViaLocation(region->getPosGlobalFromRegion(selected_parcel->getUserLocation()));
    }
}
// </FS:Ansariel>

//---------------------------------------------------------------------------
// LLPanelLandAccess
//---------------------------------------------------------------------------

LLPanelLandAccess::LLPanelLandAccess(LLParcelSelectionHandle& parcel)
    : LLPanel(),
      mParcel(parcel)
{
}


bool LLPanelLandAccess::postBuild()
{
    mPaymentInfoCheck = getChild<LLUICtrl>("limit_payment");
    mPaymentInfoCheck->setCommitCallback(onCommitAny, this);
    mAgeVerifiedCheck = getChild<LLUICtrl>("limit_age_verified");
    mAgeVerifiedCheck->setCommitCallback(onCommitAny, this);
    mTemporaryPassCheck = getChild<LLUICtrl>("PassCheck");
    mTemporaryPassCheck->setCommitCallback(onCommitAny, this);
    mPublicAccessCheck = getChild<LLUICtrl>("public_access");
    mPublicAccessCheck->setCommitCallback(onCommitPublicAccess, this);
    mGroupAccessCheck = getChild<LLUICtrl>("GroupCheck");
    mGroupAccessCheck->setCommitCallback(onCommitGroupCheck, this);
    mTemporaryPassCombo = getChild<LLComboBox>("pass_combo");
    mGroupAccessCheck->setCommitCallback(onCommitAny, this);
    mTemporaryPassPriceSpin = getChild<LLUICtrl>("PriceSpin");
    mGroupAccessCheck->setCommitCallback(onCommitAny, this);
    mTemporaryPassHourSpin = getChild<LLUICtrl>("HoursSpin");
    mGroupAccessCheck->setCommitCallback(onCommitAny, this);

    mAllowText = getChild<LLUICtrl>("AllowedText");
    mBanText = getChild<LLUICtrl>("BanCheck");

    mBtnAddAllowed = getChild<LLButton>("add_allowed");
    mBtnAddAllowed->setCommitCallback(boost::bind(&LLPanelLandAccess::onClickAddAccess, this));
    mBtnRemoveAllowed = getChild<LLButton>("remove_allowed");
    mBtnRemoveAllowed->setCommitCallback(boost::bind(&LLPanelLandAccess::onClickRemoveAccess, this));
    mBtnAddBanned = getChild<LLButton>("add_banned");
    mBtnAddBanned->setCommitCallback(boost::bind(&LLPanelLandAccess::onClickAddBanned, this));
    mBtnRemoveBanned = getChild<LLButton>("remove_banned");
    mBtnRemoveBanned->setCommitCallback(boost::bind(&LLPanelLandAccess::onClickRemoveBanned, this));

    // <FS:PP> Ban and access lists export/import
    mBtnExportAccess = getChild<LLButton>("export_allowed");
    mBtnExportAccess->setCommitCallback(boost::bind(&LLPanelLandAccess::onClickExportAccess, this));
    mBtnExportBanned = getChild<LLButton>("export_banned");
    mBtnExportBanned->setCommitCallback(boost::bind(&LLPanelLandAccess::onClickExportBanned, this));
    mBtnImportAccess = getChild<LLButton>("import_allowed");
    mBtnImportAccess->setCommitCallback(boost::bind(&LLPanelLandAccess::onClickImportAccess, this));
    mBtnImportBanned = getChild<LLButton>("import_banned");
    mBtnImportBanned->setCommitCallback(boost::bind(&LLPanelLandAccess::onClickImportBanned, this));
    // </FS:PP> Ban and access lists export/import

    mListAccess = getChild<LLNameListCtrl>("AccessList");
    if (mListAccess)
    {
        mListAccess->sortByColumnIndex(0, true); // ascending
        // <FS:Ansariel> Special Firestorm menu also allowing multi-select action
        //mListAccess->setContextMenu(LLScrollListCtrl::MENU_AVATAR);
        mListAccess->setContextMenu(&gFSNameListAvatarMenu);
        // </FS:Ansariel>
    }

    mListBanned = getChild<LLNameListCtrl>("BannedList");
    if (mListBanned)
    {
        mListBanned->sortByColumnIndex(0, true); // ascending
        // <FS:Ansariel> Special Firestorm menu also allowing multi-select action
        //mListBanned->setContextMenu(LLScrollListCtrl::MENU_AVATAR);
        mListBanned->setContextMenu(&gFSNameListAvatarMenu);
        // </FS:Ansariel>
        mListBanned->setAlternateSort();
    }

    return true;
}


LLPanelLandAccess::~LLPanelLandAccess()
{
}

void LLPanelLandAccess::refresh()
{
    LLFloater* parent_floater = gFloaterView->getParentFloater(this);
    LLParcel *parcel = mParcel->getParcel();

    // Display options
    if (parcel && !gDisconnected)
    {
        bool use_access_list = parcel->getParcelFlag(PF_USE_ACCESS_LIST);
        bool use_group = parcel->getParcelFlag(PF_USE_ACCESS_GROUP);
        bool public_access = !use_access_list;

        if (parcel->getRegionAllowAccessOverride())
        {
            mPublicAccessCheck->setValue(public_access);
            mGroupAccessCheck->setValue(use_group);
        }
        else
        {
            mPublicAccessCheck->setValue(true);
            mGroupAccessCheck->setValue(false);
        }
        std::string group_name;
        gCacheName->getGroupName(parcel->getGroupID(), group_name);
        mGroupAccessCheck->setLabelArg("[GROUP]", group_name );

        // Allow list
        if (mListAccess)
        {
            // Clear the sort order so we don't re-sort on every add.
            mListAccess->clearSortOrder();
            mListAccess->deleteAllItems();
            auto count = parcel->mAccessList.size();
            mAllowText->setTextArg("[COUNT]", llformat("%d", count));
            mAllowText->setTextArg("[MAX]", llformat("%d",PARCEL_MAX_ACCESS_LIST));

            mListAccess->setToolTipArg(LLStringExplicit("[LISTED]"), llformat("%d",count));
            mListAccess->setToolTipArg(LLStringExplicit("[MAX]"), llformat("%d",PARCEL_MAX_ACCESS_LIST));

            for (LLAccessEntry::map::const_iterator cit = parcel->mAccessList.begin();
                 cit != parcel->mAccessList.end(); ++cit)
            {
                const LLAccessEntry& entry = (*cit).second;
                std::string prefix;
                if (entry.mTime != 0)
                {
                    LLStringUtil::format_map_t args;
                    S32 now = (S32)time(NULL);
                    S32 seconds = entry.mTime - now;
                    if (seconds < 0) seconds = 0;
                    prefix.assign(" (");
                    if (seconds >= 120)
                    {
                        args["[MINUTES]"] = llformat("%d", (seconds/60));
                        std::string buf = parent_floater->getString ("Minutes", args);
                        prefix.append(buf);
                    }
                    else if (seconds >= 60)
                    {
                        prefix.append("1 " + parent_floater->getString("Minute"));
                    }
                    else
                    {
                        args["[SECONDS]"] = llformat("%d", seconds);
                        std::string buf = parent_floater->getString ("Seconds", args);
                        prefix.append(buf);
                    }
                    prefix.append(" " + parent_floater->getString("Remaining") + ") ");
                }
                mListAccess->addNameItem(entry.mID, ADD_DEFAULT, true, "", prefix);
            }
            mListAccess->sortByName(true);
        }

        // Ban List
        if(mListBanned)
        {
            // Clear the sort order so we don't re-sort on every add.
            mListBanned->clearSortOrder();
            mListBanned->deleteAllItems();
            auto count = parcel->mBanList.size();
            mBanText->setTextArg("[COUNT]", llformat("%d",count));
            mBanText->setTextArg("[MAX]", llformat("%d",PARCEL_MAX_ACCESS_LIST));

            mListBanned->setToolTipArg(LLStringExplicit("[LISTED]"), llformat("%d",count));
            mListBanned->setToolTipArg(LLStringExplicit("[MAX]"), llformat("%d",PARCEL_MAX_ACCESS_LIST));

            for (LLAccessEntry::map::const_iterator cit = parcel->mBanList.begin();
                 cit != parcel->mBanList.end(); ++cit)
            {
                const LLAccessEntry& entry = (*cit).second;
                std::string duration;
                S32 seconds = -1;
                if (entry.mTime != 0)
                {
                    LLStringUtil::format_map_t args;
                    S32 now = (S32)time(NULL);
                    seconds = entry.mTime - now;
                    if (seconds < 0) seconds = 0;

                    if (seconds >= 7200)
                    {
                        args["[HOURS]"] = llformat("%d", (seconds / 3600));
                        duration = parent_floater->getString("Hours", args);
                    }
                    else if (seconds >= 3600)
                    {
                        duration = "1 " + parent_floater->getString("Hour");
                    }
                    else if (seconds >= 120)
                    {
                        args["[MINUTES]"] = llformat("%d", (seconds / 60));
                        duration = parent_floater->getString("Minutes", args);
                    }
                    else if (seconds >= 60)
                    {
                        duration = "1 " + parent_floater->getString("Minute");
                    }
                    else
                    {
                        args["[SECONDS]"] = llformat("%d", seconds);
                        duration = parent_floater->getString("Seconds", args);
                    }
                }
                else
                {
                    duration = parent_floater->getString("Always");
                }
                LLSD item;
                item["id"] = entry.mID;
                LLSD& columns = item["columns"];
                columns[0]["column"] = "name"; // to be populated later
                columns[1]["column"] = "duration";
                columns[1]["value"] = duration;
                columns[1]["alt_value"] = entry.mTime != 0 ? std::to_string(seconds) : "Always";
                mListBanned->addElement(item);
            }
            mListBanned->sortByName(true);
        }

        if(parcel->getRegionDenyAnonymousOverride())
        {
            mPaymentInfoCheck->setValue(true);
            mPaymentInfoCheck->setLabelArg("[ESTATE_PAYMENT_LIMIT]", getString("access_estate_defined") );
        }
        else
        {
            mPaymentInfoCheck->setValue((parcel->getParcelFlag(PF_DENY_ANONYMOUS)));
            mPaymentInfoCheck->setLabelArg("[ESTATE_PAYMENT_LIMIT]", std::string() );
        }
        if(parcel->getRegionDenyAgeUnverifiedOverride())
        {
            mAgeVerifiedCheck->setValue(true);
            mAgeVerifiedCheck->setLabelArg("[ESTATE_AGE_LIMIT]", getString("access_estate_defined") );
        }
        else
        {
            mAgeVerifiedCheck->setValue((parcel->getParcelFlag(PF_DENY_AGEUNVERIFIED)));
            mAgeVerifiedCheck->setLabelArg("[ESTATE_AGE_LIMIT]", std::string() );
        }

        bool use_pass = parcel->getParcelFlag(PF_USE_PASS_LIST);
        mTemporaryPassCheck->setValue(use_pass);
        if (mTemporaryPassCombo)
        {
            if (public_access || !use_pass)
            {
                mTemporaryPassCombo->selectByValue("anyone");
            }
        }

        S32 pass_price = parcel->getPassPrice();
        mTemporaryPassPriceSpin->setValue((F32)pass_price);

        F32 pass_hours = parcel->getPassHours();
        mTemporaryPassHourSpin->setValue(pass_hours);
    }
    else
    {
        mPublicAccessCheck->setValue(false);
        mPaymentInfoCheck->setValue(false);
        mAgeVerifiedCheck->setValue(false);
        mGroupAccessCheck->setValue(false);
        mGroupAccessCheck->setLabelArg("[GROUP]", LLStringUtil::null );
        mTemporaryPassCheck->setValue(false);
        mTemporaryPassPriceSpin->setValue((F32)PARCEL_PASS_PRICE_DEFAULT);
        mTemporaryPassHourSpin->setValue(PARCEL_PASS_HOURS_DEFAULT );
        mListAccess->setToolTipArg(LLStringExplicit("[LISTED]"), llformat("%d",0));
        mListAccess->setToolTipArg(LLStringExplicit("[MAX]"), llformat("%d",0));
        mListBanned->setToolTipArg(LLStringExplicit("[LISTED]"), llformat("%d",0));
        mListBanned->setToolTipArg(LLStringExplicit("[MAX]"), llformat("%d",0));
        // <FS:Ansariel> FIRE-9211: Add counter to parcel ban and access lists
        mAllowText->setTextArg(LLStringExplicit("[LISTED]"), llformat("%d",0));
        mAllowText->setTextArg(LLStringExplicit("[MAX]"), llformat("%d",0));
        mBanText->setTextArg(LLStringExplicit("[LISTED]"), llformat("%d",0));
        mBanText->setTextArg(LLStringExplicit("[MAX]"), llformat("%d",0));
        // </FS:Ansariel>
    }
}

void LLPanelLandAccess::refresh_ui()
{
    mPublicAccessCheck->setEnabled(false);
    mPaymentInfoCheck->setEnabled(false);
    mAgeVerifiedCheck->setEnabled(false);
    mGroupAccessCheck->setEnabled(false);
    mTemporaryPassCheck->setEnabled(false);
    mTemporaryPassCombo->setEnabled(false);
    mTemporaryPassPriceSpin->setEnabled(false);
    mTemporaryPassHourSpin->setEnabled(false);
    mListAccess->setEnabled(false);
    mListBanned->setEnabled(false);
    mBtnAddAllowed->setEnabled(false);
    mBtnRemoveAllowed->setEnabled(false);
    mBtnAddBanned->setEnabled(false);
    mBtnRemoveBanned->setEnabled(false);

    // <FS:PP> Ban and access lists export/import
    mBtnExportAccess->setEnabled(false);
    mBtnExportBanned->setEnabled(false);
    mBtnImportAccess->setEnabled(false);
    mBtnImportBanned->setEnabled(false);
    // </FS:PP> Ban and access lists export/import

    LLParcel *parcel = mParcel->getParcel();
    if (parcel && !gDisconnected)
    {
        bool can_manage_allowed = false;
        bool can_manage_banned = LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_LAND_MANAGE_BANNED);

        if (parcel->getRegionAllowAccessOverride())
        {   // Estate owner may have disabled allowing the parcel owner from managing access.
            can_manage_allowed = LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_LAND_MANAGE_ALLOWED);
        }

        mPublicAccessCheck->setEnabled(can_manage_allowed);
        bool public_access = mPublicAccessCheck->getValue().asBoolean();
        if (public_access)
        {
            // bool override = false; // <FS:Beq/> set but never used clang appeasement
            if(parcel->getRegionDenyAnonymousOverride())
            {
                // override = true; // <FS:Beq/> set but never used clang appeasement
                mPaymentInfoCheck->setEnabled(false);
            }
            else
            {
                mPaymentInfoCheck->setEnabled(can_manage_allowed);
            }
            if(parcel->getRegionDenyAgeUnverifiedOverride())
            {
                // override = true; // <FS:Beq/> set but never used clang appeasement
                mAgeVerifiedCheck->setEnabled(false);
            }
            else
            {
                mAgeVerifiedCheck->setEnabled(can_manage_allowed);
            }
            mTemporaryPassCheck->setEnabled(false);
            mTemporaryPassCombo->setEnabled(false);
            mListAccess->setEnabled(false);
        }
        else
        {
            mPaymentInfoCheck->setEnabled(false);
            mAgeVerifiedCheck->setEnabled(false);

            bool sell_passes = mTemporaryPassCheck->getValue().asBoolean();
            mTemporaryPassCheck->setEnabled(can_manage_allowed);
            if (sell_passes)
            {
                mTemporaryPassCombo->setEnabled(can_manage_allowed);
                mTemporaryPassPriceSpin->setEnabled(can_manage_allowed);
                mTemporaryPassHourSpin->setEnabled(can_manage_allowed);
            }
        }
        std::string group_name;
        if (gCacheName->getGroupName(parcel->getGroupID(), group_name))
        {
            bool can_allow_groups = !public_access || (public_access && (mPaymentInfoCheck->getValue().asBoolean() ^ mAgeVerifiedCheck->getValue().asBoolean()));
            mGroupAccessCheck->setEnabled(can_manage_allowed && can_allow_groups);
        }
        mListAccess->setEnabled(can_manage_allowed);
        auto allowed_list_count = parcel->mAccessList.size();
        mBtnAddAllowed->setEnabled(can_manage_allowed && allowed_list_count < PARCEL_MAX_ACCESS_LIST);
        bool has_selected = (mListAccess && mListAccess->getSelectionInterface()->getFirstSelectedIndex() >= 0);
        mBtnRemoveAllowed->setEnabled(can_manage_allowed && has_selected);

        mListBanned->setEnabled(can_manage_banned);
        auto banned_list_count = parcel->mBanList.size();
        mBtnAddBanned->setEnabled(can_manage_banned && banned_list_count < PARCEL_MAX_ACCESS_LIST);
        has_selected = (mListBanned && mListBanned->getSelectionInterface()->getFirstSelectedIndex() >= 0);
        mBtnRemoveBanned->setEnabled(can_manage_banned && has_selected);

        // <FS:PP> Ban and access lists export/import
        mBtnExportAccess->setEnabled(can_manage_allowed && allowed_list_count > 0);
        mBtnExportBanned->setEnabled(can_manage_banned && banned_list_count > 0);
        mBtnImportAccess->setEnabled(can_manage_allowed && allowed_list_count < PARCEL_MAX_ACCESS_LIST);
        mBtnImportBanned->setEnabled(can_manage_banned && banned_list_count < PARCEL_MAX_ACCESS_LIST);
        // </FS:PP> Ban and access lists export/import

    }
}


// public
void LLPanelLandAccess::refreshNames()
{
    LLParcel* parcel = mParcel->getParcel();
    std::string group_name;
    if(parcel)
    {
        gCacheName->getGroupName(parcel->getGroupID(), group_name);
    }
    mGroupAccessCheck->setLabelArg("[GROUP]", group_name);
}


// virtual
void LLPanelLandAccess::draw()
{
    refresh_ui();
    refreshNames();
    LLPanel::draw();
}

// static
void LLPanelLandAccess::onCommitPublicAccess(LLUICtrl *ctrl, void *userdata)
{
    LLPanelLandAccess *self = (LLPanelLandAccess *)userdata;
    LLParcel* parcel = self->mParcel->getParcel();
    if (!parcel)
    {
        return;
    }

    // <FS:Ansariel> FIRE-12551: Enable group access by default if public access is removed
    //                           or we might end up banning ourself from the land!
    bool public_access = self->getChild<LLUICtrl>("public_access")->getValue().asBoolean();
    if (!public_access)
    {
        std::string group_name;
        if (gCacheName->getGroupName(parcel->getGroupID(), group_name))
        {
            self->getChild<LLUICtrl>("GroupCheck")->setValue(public_access ? false : true);
        }
    }
    // </FS:Ansariel>

    onCommitAny(ctrl, userdata);
}

void LLPanelLandAccess::onCommitGroupCheck(LLUICtrl *ctrl, void *userdata)
{
    LLPanelLandAccess *self = (LLPanelLandAccess *)userdata;
    LLParcel* parcel = self->mParcel->getParcel();
    if (!parcel)
    {
        return;
    }

    bool use_pass_list = !self->mPublicAccessCheck->getValue().asBoolean();
    bool use_access_group = self->mGroupAccessCheck->getValue().asBoolean();
    LLCtrlSelectionInterface* passcombo = self->mTemporaryPassCombo;
    if (passcombo)
    {
        if (use_access_group && use_pass_list)
        {
            if (passcombo->getSelectedValue().asString() == "group")
            {
                passcombo->selectByValue("anyone");
            }
        }
    }

    onCommitAny(ctrl, userdata);
}

// static
void LLPanelLandAccess::onCommitAny(LLUICtrl *ctrl, void *userdata)
{
    LLPanelLandAccess *self = (LLPanelLandAccess *)userdata;

    LLParcel* parcel = self->mParcel->getParcel();
    if (!parcel)
    {
        return;
    }

    // Extract data from UI
    bool public_access = self->mPublicAccessCheck->getValue().asBoolean();
    bool use_access_group = self->mGroupAccessCheck->getValue().asBoolean();
    if (use_access_group)
    {
        std::string group_name;
        if (!gCacheName->getGroupName(parcel->getGroupID(), group_name))
        {
            use_access_group = false;
        }
    }

    bool limit_payment = false, limit_age_verified = false;
    bool use_access_list = false;
    bool use_pass_list = false;

    if (public_access)
    {
        use_access_list = false;
        limit_payment = self->mPaymentInfoCheck->getValue().asBoolean();
        limit_age_verified = self->mAgeVerifiedCheck->getValue().asBoolean();
    }
    else
    {
        use_access_list = true;
        use_pass_list = self->mTemporaryPassCheck->getValue().asBoolean();
        LLCtrlSelectionInterface* passcombo = self->mTemporaryPassCombo;
        if (passcombo)
        {
            if (use_access_group && use_pass_list)
            {
                if (passcombo->getSelectedValue().asString() == "group")
                {
                    use_access_group = false;
                }
            }
        }
    }

    S32 pass_price = llfloor((F32)self->mTemporaryPassPriceSpin->getValue().asReal());
    F32 pass_hours = (F32)self->mTemporaryPassHourSpin->getValue().asReal();

    // Push data into current parcel
    parcel->setParcelFlag(PF_USE_ACCESS_GROUP,  use_access_group);
    parcel->setParcelFlag(PF_USE_ACCESS_LIST,   use_access_list);
    parcel->setParcelFlag(PF_USE_PASS_LIST,     use_pass_list);
    parcel->setParcelFlag(PF_USE_BAN_LIST,      true);
    parcel->setParcelFlag(PF_DENY_ANONYMOUS,    limit_payment);
    parcel->setParcelFlag(PF_DENY_AGEUNVERIFIED, limit_age_verified);

    parcel->setPassPrice( pass_price );
    parcel->setPassHours( pass_hours );

    // Send current parcel data upstream to server
    LLViewerParcelMgr::getInstance()->sendParcelPropertiesUpdate( parcel );

    // Might have changed properties, so let's redraw!
    self->refresh();
}

void LLPanelLandAccess::onClickAddAccess()
{
    LLFloater * root_floater = gFloaterView->getParentFloater(this);
    LLFloaterAvatarPicker* picker = LLFloaterAvatarPicker::show(
        boost::bind(&LLPanelLandAccess::callbackAvatarCBAccess, this, _1), false, false, false, root_floater->getName(), mBtnAddAllowed);
    if (picker)
    {
        root_floater->addDependentFloater(picker);
    }
}

void LLPanelLandAccess::callbackAvatarCBAccess(const uuid_vec_t& ids)
{
    if (!ids.empty())
    {
        LLUUID id = ids[0];
        LLParcel* parcel = mParcel->getParcel();
        if (parcel && parcel->addToAccessList(id, 0))
        {
            U32 lists_to_update = AL_ACCESS;
            // agent was successfully added to access list
            // but we also need to check ban list to ensure that agent will not be in two lists simultaneously
            if(parcel->removeFromBanList(id))
            {
                lists_to_update |= AL_BAN;
            }
            LLViewerParcelMgr::getInstance()->sendParcelAccessListUpdate(lists_to_update);
            refresh();
        }
    }
}

void LLPanelLandAccess::onClickRemoveAccess()
{
    if (mListAccess)
    {
        LLParcel* parcel = mParcel->getParcel();
        if (parcel)
        {
            std::vector<LLScrollListItem*> names = mListAccess->getAllSelected();
            for (std::vector<LLScrollListItem*>::iterator iter = names.begin();
                 iter != names.end(); )
            {
                LLScrollListItem* item = *iter++;
                const LLUUID& agent_id = item->getUUID();
                parcel->removeFromAccessList(agent_id);
            }
            LLViewerParcelMgr::getInstance()->sendParcelAccessListUpdate(AL_ACCESS);
            refresh();
        }
    }
}

void LLPanelLandAccess::onClickAddBanned()
{
    LLFloater * root_floater = gFloaterView->getParentFloater(this);
    LLFloaterAvatarPicker* picker = LLFloaterAvatarPicker::show(
        boost::bind(&LLPanelLandAccess::callbackAvatarCBBanned, this, _1), true, false, false, root_floater->getName(), mBtnAddBanned);
    if (picker)
    {
        root_floater->addDependentFloater(picker);
    }
}

// static
void LLPanelLandAccess::callbackAvatarCBBanned(const uuid_vec_t& ids)
{
    LLFloater * root_floater = gFloaterView->getParentFloater(this);
    LLFloaterBanDuration* duration_floater = LLFloaterBanDuration::show(
        boost::bind(&LLPanelLandAccess::callbackAvatarCBBanned2, this, _1, _2), ids);
    if (duration_floater)
    {
        root_floater->addDependentFloater(duration_floater);
    }
}

void LLPanelLandAccess::callbackAvatarCBBanned2(const uuid_vec_t& ids, S32 duration)
{
    LLParcel* parcel = mParcel->getParcel();
    if (!parcel) return;

    U32 lists_to_update = 0;

    for (uuid_vec_t::const_iterator it = ids.begin(); it < ids.end(); it++)
    {
        LLUUID id = *it;
        if (parcel->addToBanList(id, duration))
        {
            lists_to_update |= AL_BAN;
            // agent was successfully added to ban list
            // but we also need to check access list to ensure that agent will not be in two lists simultaneously
            if (parcel->removeFromAccessList(id))
            {
                lists_to_update |= AL_ACCESS;
            }
        }
    }
    if (lists_to_update > 0)
    {
        LLViewerParcelMgr::getInstance()->sendParcelAccessListUpdate(lists_to_update);
        refresh();
    }
}

void LLPanelLandAccess::onClickRemoveBanned()
{
    if (mListBanned)
    {
        LLParcel* parcel = mParcel->getParcel();
        if (parcel)
        {
            std::vector<LLScrollListItem*> names = mListBanned->getAllSelected();
            for (std::vector<LLScrollListItem*>::iterator iter = names.begin();
                 iter != names.end(); )
            {
                LLScrollListItem* item = *iter++;
                const LLUUID& agent_id = item->getUUID();
                parcel->removeFromBanList(agent_id);
            }
            LLViewerParcelMgr::getInstance()->sendParcelAccessListUpdate(AL_BAN);
            refresh();
        }
    }
}

// <FS:PP> Ban and access lists export/import
void LLPanelLandAccess::onClickExportList(LLNameListCtrl* list, const std::string& filename)
{
    if (!list || list->getItemCount() == 0)
    {
        LLSD args;
        args["MESSAGE"] = LLTrans::getString("ListEmpty");
        LLNotificationsUtil::add("GenericAlert", args);
        return;
    }
    LLFilePickerReplyThread::startPicker(boost::bind(&LLPanelLandAccess::exportListCallback, this, list, _1), LLFilePicker::FFSAVE_CSV, filename);
}

void LLPanelLandAccess::onClickExportAccess()
{
    onClickExportList(mListAccess, "land_access_list.csv");
}

void LLPanelLandAccess::onClickExportBanned()
{
    onClickExportList(mListBanned, "land_banned_list.csv");
}

void LLPanelLandAccess::exportListCallback(LLNameListCtrl* list, const std::vector<std::string>& filenames)
{
    if (filenames.empty())
    {
        return;
    }

    std::string filename = filenames[0];
    llofstream file(filename.c_str());
    if (!file.is_open())
    {
        LLNotificationsUtil::add("ExportFailed");
        return;
    }

    file << "Name,UUID\n";
    std::vector<LLScrollListItem*> items = list->getAllData();
    for (std::vector<LLScrollListItem*>::iterator it = items.begin(); it != items.end(); ++it)
    {
        LLScrollListItem* item = *it;
        if (item)
        {
            const LLUUID& id = item->getUUID();
            std::string name = item->getColumn(0)->getValue().asString();
            file << name << "," << id.asString() << "\n";
        }
    }
    file.close();

    LLSD args;
    args["FILENAME"] = filename;
    LLNotificationsUtil::add("ExportFinished", args);
}

void LLPanelLandAccess::onClickImportList(LLNameListCtrl* list)
{
    if (!list)
    {
        LLSD args;
        args["MESSAGE"] = LLTrans::getString("ListEmpty");
        LLNotificationsUtil::add("GenericAlert", args);
        return;
    }
    LLFilePickerReplyThread::startPicker(boost::bind(&LLPanelLandAccess::importListCallback, this, list, _1), LLFilePicker::FFLOAD_ALL, false);
}

void LLPanelLandAccess::onClickImportAccess()
{
    onClickImportList(mListAccess);
}

void LLPanelLandAccess::onClickImportBanned()
{
    onClickImportList(mListBanned);
}

void LLPanelLandAccess::importListCallback(LLNameListCtrl* list, const std::vector<std::string>& filenames)
{
    if (filenames.empty())
    {
        return;
    }

    std::string filename = filenames[0];

    llifstream file(filename.c_str());
    if (!file.is_open())
    {
        return;
    }

    LLParcel* parcel = mParcel->getParcel();
    if (!parcel)
    {
        return;
    }

    uuid_vec_t uuids;
    LLSD       csvData = ll_sd_from_csv(file);
    file.close();

    for (const auto& entry : llsd::inArray(csvData))
    {
        if (entry.has("UUID"))
        {
            LLUUID id{ entry["UUID"].asUUID() };
            if (id.notNull())
                uuids.push_back(std::move(id));
        }
    }

    if (uuids.empty())
    {
        LLSD args;
        args["MESSAGE"] = LLTrans::getString("NoValidUUIDs");
        LLNotificationsUtil::add("GenericAlert", args);
        return;
    }

    std::string listname = list->getName();
    S32 max_entries = PARCEL_MAX_ACCESS_LIST;
    S32 current_count = list->getItemCount();
    S32 available_slots = max_entries - current_count;

    if (uuids.size() > available_slots)
    {
        LLSD args;
        args["MAX"] = llformat("%d", available_slots);
        args["COUNT"] = llformat("%d", uuids.size());
        args["MESSAGE"] = LLTrans::getString("ImportListTooLarge", args).c_str();
        LLNotificationsUtil::add("GenericAlert", args);
        return;
    }

    for (const LLUUID& uuid : uuids)
    {
        if (listname == "BannedList")
        {
            parcel->addToBanList(uuid, 0);
        }
        else
        {
            parcel->addToAccessList(uuid, 0);
        }
    }

    U32 flags = (listname == "BannedList") ? AL_BAN : AL_ACCESS;
    LLViewerParcelMgr::getInstance()->sendParcelAccessListUpdate(flags);
    refresh();

    LLSD args;
    args["COUNT"] = llformat("%d", uuids.size());
    args["MESSAGE"] = LLTrans::getString("ImportSuccessful", args).c_str();
    LLNotificationsUtil::add("GenericAlert", args);
}
// </FS:PP> Ban and access lists export/import

//---------------------------------------------------------------------------
// LLPanelLandCovenant
//---------------------------------------------------------------------------
LLPanelLandCovenant::LLPanelLandCovenant(LLParcelSelectionHandle& parcel)
    : LLPanel(),
      mParcel(parcel),
      mNextUpdateTime(0)
{
}

LLPanelLandCovenant::~LLPanelLandCovenant()
{
}

bool LLPanelLandCovenant::postBuild()
{
    mLastRegionID = LLUUID::null;
    mNextUpdateTime = 0;
    mTextEstateOwner = getChild<LLTextBox>("estate_owner_text");
    mTextEstateOwner->setIsFriendCallback(LLAvatarActions::isFriend);
    return true;
}

// virtual
void LLPanelLandCovenant::refresh()
{
    LLViewerRegion* region = LLViewerParcelMgr::getInstance()->getSelectionRegion();
    if(!region || gDisconnected) return;

    LLTextBox* region_name = getChild<LLTextBox>("region_name_text");
    if (region_name)
    {
        region_name->setText(region->getName());
    }

    LLTextBox* region_landtype = getChild<LLTextBox>("region_landtype_text");
    region_landtype->setText(region->getLocalizedSimProductName());

    LLTextBox* region_maturity = getChild<LLTextBox>("region_maturity_text");
    if (region_maturity)
    {
        insert_maturity_into_textbox(region_maturity, gFloaterView->getParentFloater(this), MATURITY);
    }

    LLTextBox* resellable_clause = getChild<LLTextBox>("resellable_clause");
    if (resellable_clause)
    {
        if (region->getRegionFlag(REGION_FLAGS_BLOCK_LAND_RESELL))
        {
            resellable_clause->setText(getString("can_not_resell"));
        }
        else
        {
            resellable_clause->setText(getString("can_resell"));
        }
    }

    LLTextBox* changeable_clause = getChild<LLTextBox>("changeable_clause");
    if (changeable_clause)
    {
        if (region->getRegionFlag(REGION_FLAGS_ALLOW_PARCEL_CHANGES))
        {
            changeable_clause->setText(getString("can_change"));
        }
        else
        {
            changeable_clause->setText(getString("can_not_change"));
        }
    }

    if (mLastRegionID != region->getRegionID()
        || mNextUpdateTime < LLTimer::getElapsedSeconds())
    {
        // Request Covenant Info
        // Note: LLPanelLandCovenant doesn't change Covenant's content and any
        // changes made by Estate floater should be requested by Estate floater
        LLMessageSystem *msg = gMessageSystem;
        msg->newMessage("EstateCovenantRequest");
        msg->nextBlockFast(_PREHASH_AgentData);
        msg->addUUIDFast(_PREHASH_AgentID,  gAgent.getID());
        msg->addUUIDFast(_PREHASH_SessionID,gAgent.getSessionID());
        msg->sendReliable(region->getHost());

        mLastRegionID = region->getRegionID();
        mNextUpdateTime = LLTimer::getElapsedSeconds() + COVENANT_REFRESH_TIME_SEC;
    }
}

// static
void LLPanelLandCovenant::updateCovenant(const LLTextBase* source)
{
    if (LLPanelLandCovenant* self = LLFloaterLand::getCurrentPanelLandCovenant())
    {
        LLViewerTextEditor* editor = self->getChild<LLViewerTextEditor>("covenant_editor");
        editor->copyContents(source);
    }
}

// static
void LLPanelLandCovenant::updateCovenantText(const std::string &string)
{
    LLPanelLandCovenant* self = LLFloaterLand::getCurrentPanelLandCovenant();
    if (self)
    {
        LLViewerTextEditor* editor = self->getChild<LLViewerTextEditor>("covenant_editor");
        editor->setText(string);
    }
}

// static
void LLPanelLandCovenant::updateEstateName(const std::string& name)
{
    LLPanelLandCovenant* self = LLFloaterLand::getCurrentPanelLandCovenant();
    if (self)
    {
        LLTextBox* editor = self->getChild<LLTextBox>("estate_name_text");
        if (editor) editor->setText(name);
    }
}

// static
void LLPanelLandCovenant::updateLastModified(const std::string& text)
{
    LLPanelLandCovenant* self = LLFloaterLand::getCurrentPanelLandCovenant();
    if (self)
    {
        LLTextBox* editor = self->getChild<LLTextBox>("covenant_timestamp_text");
        if (editor) editor->setText(text);
    }
}

// static
void LLPanelLandCovenant::updateEstateOwnerName(const std::string& name)
{
    LLPanelLandCovenant* self = LLFloaterLand::getCurrentPanelLandCovenant();
    if (self)
    {
        self->mTextEstateOwner->setText(name);
    }
}

// inserts maturity info(icon and text) into target textbox
// names_floater - pointer to floater which contains strings with maturity icons filenames
// str_to_parse is string in format "txt1[MATURITY]txt2" where maturity icon and text will be inserted instead of [MATURITY]
void insert_maturity_into_textbox(LLTextBox* target_textbox, LLFloater* names_floater, std::string str_to_parse)
{
    LLViewerRegion* region = LLViewerParcelMgr::getInstance()->getSelectionRegion();
    if (!region)
        return;

    LLStyle::Params style;

    U8 sim_access = region->getSimAccess();

    switch(sim_access)
    {
    case SIM_ACCESS_PG:
        style.image(LLUI::getUIImage(names_floater->getString("maturity_icon_general")));
        break;

    case SIM_ACCESS_ADULT:
        style.image(LLUI::getUIImage(names_floater->getString("maturity_icon_adult")));
        break;

    case SIM_ACCESS_MATURE:
        style.image(LLUI::getUIImage(names_floater->getString("maturity_icon_moderate")));
        break;

    default:
        break;
    }

    size_t maturity_pos = str_to_parse.find(MATURITY);

    if (maturity_pos == std::string::npos)
    {
        return;
    }

    std::string text_before_rating = str_to_parse.substr(0, maturity_pos);
    std::string text_after_rating = str_to_parse.substr(maturity_pos + MATURITY.length());

    target_textbox->setText(text_before_rating);

    target_textbox->appendImageSegment(style);

    target_textbox->appendText(LLViewerParcelMgr::getInstance()->getSelectionRegion()->getSimAccessString(), false);
    target_textbox->appendText(text_after_rating, false);
}

LLPanelLandExperiences::LLPanelLandExperiences( LLSafeHandle<LLParcelSelection>& parcelp )
    : mParcel(parcelp)
{

}


bool LLPanelLandExperiences::postBuild()
{
    mAllowed = setupList("panel_allowed", EXPERIENCE_KEY_TYPE_ALLOWED, AL_ALLOW_EXPERIENCE);
    mBlocked = setupList("panel_blocked", EXPERIENCE_KEY_TYPE_BLOCKED, AL_BLOCK_EXPERIENCE);

    // only non-grid-wide experiences
    mAllowed->addFilter(boost::bind(LLPanelExperiencePicker::FilterWithProperty, _1, LLExperienceCache::PROPERTY_GRID));

    // no privileged ones
    mBlocked->addFilter(boost::bind(LLPanelExperiencePicker::FilterWithoutProperties, _1, LLExperienceCache::PROPERTY_PRIVILEGED|LLExperienceCache::PROPERTY_GRID));

    getChild<LLLayoutPanel>("trusted_layout_panel")->setVisible(false);
    getChild<LLTextBox>("experiences_help_text")->setVisible(false);
    getChild<LLTextBox>("allowed_text_help")->setText(getString("allowed_parcel_text"));
    getChild<LLTextBox>("blocked_text_help")->setText(getString("blocked_parcel_text"));

    return LLPanel::postBuild();
}

LLPanelExperienceListEditor* LLPanelLandExperiences::setupList( const char* control_name, U32 xp_type, U32 access_type )
{
    LLPanelExperienceListEditor* child = findChild<LLPanelExperienceListEditor>(control_name);
    if(child)
    {
        child->getChild<LLTextBox>("text_name")->setText(child->getString(control_name));
        child->setMaxExperienceIDs(PARCEL_MAX_EXPERIENCE_LIST);
        child->setAddedCallback(boost::bind(&LLPanelLandExperiences::experienceAdded, this, _1, xp_type, access_type));
        child->setRemovedCallback(boost::bind(&LLPanelLandExperiences::experienceRemoved, this, _1, access_type));
    }

    return child;
}

void LLPanelLandExperiences::experienceAdded( const LLUUID& id, U32 xp_type, U32 access_type )
{
    LLParcel* parcel = mParcel->getParcel();
    if (parcel)
    {
        parcel->setExperienceKeyType(id, xp_type);
        LLViewerParcelMgr::getInstance()->sendParcelAccessListUpdate(access_type);
        refresh();
    }
}

void LLPanelLandExperiences::experienceRemoved( const LLUUID& id, U32 access_type )
{
    LLParcel* parcel = mParcel->getParcel();
    if (parcel)
    {
        parcel->setExperienceKeyType(id, EXPERIENCE_KEY_TYPE_NONE);
        LLViewerParcelMgr::getInstance()->sendParcelAccessListUpdate(access_type);
        refresh();
    }
}

void LLPanelLandExperiences::refreshPanel(LLPanelExperienceListEditor* panel, U32 xp_type)
{
    LLParcel *parcel = mParcel->getParcel();

    // Display options
    if (panel == NULL)
    {
        return;
    }
    if (!parcel || gDisconnected)
    {
        // disable the panel
        panel->setEnabled(false);
        panel->setExperienceIds(LLSD::emptyArray());
    }
    else
    {
        // enable the panel
        panel->setEnabled(true);
        LLAccessEntry::map entries = parcel->getExperienceKeysByType(xp_type);
        LLAccessEntry::map::iterator it = entries.begin();
        LLSD ids = LLSD::emptyArray();
        for (/**/; it != entries.end(); ++it)
        {
            ids.append(it->second.mID);
        }
        panel->setExperienceIds(ids);
        panel->setReadonly(!LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_LAND_OPTIONS));
        panel->refreshExperienceCounter();
    }
}

void LLPanelLandExperiences::refresh()
{
    refreshPanel(mAllowed, EXPERIENCE_KEY_TYPE_ALLOWED);
    refreshPanel(mBlocked, EXPERIENCE_KEY_TYPE_BLOCKED);
}

//=========================================================================

LLPanelLandEnvironment::LLPanelLandEnvironment(LLParcelSelectionHandle& parcel) :
    LLPanelEnvironmentInfo(),
    mParcel(parcel),
    mLastParcelId(INVALID_PARCEL_ID)
{
}

bool LLPanelLandEnvironment::postBuild()
{
    if (!LLPanelEnvironmentInfo::postBuild())
        return false;

    mBtnUseDefault->setLabelArg("[USEDEFAULT]", getString(STR_LABEL_USEREGION));
    mCheckAllowOverride->setVisible(false);
    mPanelEnvRegionMsg->setVisible(false);
    mPanelEnvAltitudes->setVisible(true);

    return true;
}

void LLPanelLandEnvironment::refresh()
{
    if (gDisconnected)
        return;

    commitDayLenOffsetChanges(false); // commit unsaved changes if any

    if (!isSameRegion())
    {
        setCrossRegion(true);
        mCurrentEnvironment.reset();
        mLastParcelId = INVALID_PARCEL_ID;
        mCurEnvVersion = INVALID_PARCEL_ENVIRONMENT_VERSION;
        setControlsEnabled(false);
        return;
    }

    if (mLastParcelId != getParcelId())
    {
        mCurEnvVersion = INVALID_PARCEL_ENVIRONMENT_VERSION;
        mCurrentEnvironment.reset();
    }

    if (!mCurrentEnvironment && mCurEnvVersion <= INVALID_PARCEL_ENVIRONMENT_VERSION)
    {
        refreshFromSource();
        return;
    }

    LLPanelEnvironmentInfo::refresh();
}

void LLPanelLandEnvironment::refreshFromSource()
{
    LLParcel *parcel = getParcel();

    if (!LLEnvironment::instance().isExtendedEnvironmentEnabled())
    {
        setNoEnvironmentSupport(true);
        setControlsEnabled(false);
        mCurEnvVersion = INVALID_PARCEL_ENVIRONMENT_VERSION;
        return;
    }
    setNoEnvironmentSupport(false);

    if (!parcel)
    {
        setNoSelection(true);
        setControlsEnabled(false);
        mCurEnvVersion = INVALID_PARCEL_ENVIRONMENT_VERSION;
        return;
    }

    setNoSelection(false);
    if (isSameRegion())
    {
        LL_DEBUGS("ENVIRONMENT") << "Requesting environment for parcel " << parcel->getLocalID() << ", known version " << mCurEnvVersion << LL_ENDL;
        setCrossRegion(false);

        LLHandle<LLPanel> that_h = getHandle();

        if (mCurEnvVersion < UNSET_PARCEL_ENVIRONMENT_VERSION)
        {
            // to mark as requesting
            mCurEnvVersion = parcel->getParcelEnvironmentVersion();
        }
        mLastParcelId = parcel->getLocalID();

        LLEnvironment::instance().requestParcel(parcel->getLocalID(),
            [that_h](S32 parcel_id, LLEnvironment::EnvironmentInfo::ptr_t envifo)
            {
                LLPanelLandEnvironment *that = (LLPanelLandEnvironment*)that_h.get();
                if (!that) return;
                that->mLastParcelId = parcel_id;
                that->onEnvironmentReceived(parcel_id, envifo);
            });
    }
    else
    {
        setCrossRegion(true);
        mCurrentEnvironment.reset();
        mLastParcelId = INVALID_PARCEL_ID;
        mCurEnvVersion = INVALID_PARCEL_ENVIRONMENT_VERSION;
    }
    setControlsEnabled(false);
}


bool LLPanelLandEnvironment::isSameRegion()
{
    LLViewerRegion* regionp = LLViewerParcelMgr::instance().getSelectionRegion();

    return (!regionp || (regionp->getRegionID() == gAgent.getRegion()->getRegionID()));
}

LLParcel *LLPanelLandEnvironment::getParcel()
{
    return mParcel->getParcel();
}


bool LLPanelLandEnvironment::canEdit()
{
    LLParcel *parcel = getParcel();
    if (!parcel)
        return false;

    return LLEnvironment::instance().canAgentUpdateParcelEnvironment(parcel) && mAllowOverride;
}

S32 LLPanelLandEnvironment::getParcelId()
{
    LLParcel *parcel = getParcel();
    if (!parcel)
        return INVALID_PARCEL_ID;

    return parcel->getLocalID();
}
