/**
 * @file llviewerchat.cpp
 * @brief Builds menus out of items.
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
#include "llviewerchat.h"

// newview includes
#include "llagent.h"    // gAgent
#include "llslurl.h"
#include "lluicolor.h"
#include "lluicolortable.h"
#include "llviewercontrol.h" // gSavedSettings
#include "llviewerregion.h"
#include "llworld.h"
#include "llinstantmessage.h" //SYSTEM_FROM
#include "llurlregistry.h"
#include "fskeywords.h"
#include "lggcontactsets.h"
#include "rlvhandler.h"

#include "lfsimfeaturehandler.h"    // <FS:CR> Opensim


// LLViewerChat
LLViewerChat::font_change_signal_t LLViewerChat::sChatFontChangedSignal;

//static
// <FS:Ansariel> Add additional options
//void LLViewerChat::getChatColor(const LLChat& chat, LLUIColor& r_color, F32& r_color_alpha)
//{
void LLViewerChat::getChatColor(const LLChat& chat, LLUIColor& r_color, F32& r_color_alpha, LLSD args)
{
    const bool is_local = args.has("is_local") ? args["is_local"].asBoolean() : true;
    const bool for_console = args.has("for_console") && args["for_console"].asBoolean();
// </FS:Ansariel>

    // <FS:Ansariel> FIRE-1061 - Color friends, lindens, muted, etc
    //if(chat.mMuted)
    //{
    //  r_color= LLUIColorTable::instance().getColor("LtGray");
    //}
    //else
    // </FS:Ansariel>
    {
        switch(chat.mSourceType)
        {
            case CHAT_SOURCE_TELEPORT: // <FS:Ansariel> FIRE-31034: New teleport info system message ignoring system chat color
            case CHAT_SOURCE_SYSTEM:
                r_color = LLUIColorTable::instance().getColor("SystemChatColor");
                break;
            case CHAT_SOURCE_AGENT:
                if (chat.mFromID.isNull() || SYSTEM_FROM == chat.mFromName)
                {
                    r_color = LLUIColorTable::instance().getColor("SystemChatColor");
                }
                else
                {
                    // <FS:CR> FIRE-1061 - Color friends, lindens, muted, etc
                    // Handle "UserChatColor" through the colorizer
                    //if(gAgentID == chat.mFromID)
                    //{
                    //  r_color = LLUIColorTable::instance().getColor("UserChatColor");
                    //}
                    //else
                    //{
                    //  r_color = LLUIColorTable::instance().getColor("AgentChatColor");
                    //}
                    static LLCachedControl<bool> im_coloring(gSavedSettings, "FSColorIMsDistinctly");
                    if (for_console && im_coloring)
                    {
                        r_color = LLUIColorTable::instance().getColor("AgentIMColor");
                    }
                    else
                    {
                        r_color = LLUIColorTable::instance().getColor("AgentChatColor");
                    }

                    if (chat.mChatType == CHAT_TYPE_IM || chat.mChatType == CHAT_TYPE_IM_GROUP)
                    {
                        r_color = LGGContactSets::getInstance()->colorize(chat.mFromID, r_color, ContactSetType::IM);
                    }
                    else
                    {
                        r_color = LGGContactSets::getInstance()->colorize(chat.mFromID, r_color, ContactSetType::CHAT);
                    }
                    // </FS:CR>

                    //color based on contact sets prefs
                    if (LLColor4 cscolor; LGGContactSets::getInstance()->hasFriendColorThatShouldShow(chat.mFromID, ContactSetType::CHAT, cscolor))
                    {
                        r_color = cscolor;
                    }
                }
                break;
            case CHAT_SOURCE_OBJECT:
                if (chat.mChatType == CHAT_TYPE_DEBUG_MSG)
                {
                    r_color = LLUIColorTable::instance().getColor("ScriptErrorColor");
                }
                else if ( chat.mChatType == CHAT_TYPE_OWNER )
                {
                    r_color = LLUIColorTable::instance().getColor("llOwnerSayChatColor");
                }
                else if ( chat.mChatType == CHAT_TYPE_DIRECT )
                {
                    r_color = LLUIColorTable::instance().getColor("DirectChatColor");
                }
                else if ( chat.mChatType == CHAT_TYPE_IM )
                {
                    r_color = LLUIColorTable::instance().getColor("ObjectIMColor");
                }
                else
                {
                    r_color = LLUIColorTable::instance().getColor("ObjectChatColor");
                }
                break;
            default:
                r_color = LLUIColorTable::instance().getColor("White");
        }

        // <FS:KC> Keyword alerts
        static LLCachedControl<LLColor4> sFSKeywordColor(gSavedPerAccountSettings, "FSKeywordColor");
        static LLCachedControl<bool> sFSKeywordChangeColor(gSavedPerAccountSettings, "FSKeywordChangeColor");
        if (sFSKeywordChangeColor && FSKeywords::getInstance()->chatContainsKeyword(chat, is_local))
        {
            r_color = sFSKeywordColor();
        }
        // </FS:KC>

        if (!chat.mPosAgent.isExactlyZero())
        {
            LLVector3 pos_agent = gAgent.getPositionAgent();
            F32 distance_squared = dist_vec_squared(pos_agent, chat.mPosAgent);
// <FS:CR> Opensim
            //F32 dist_near_chat = gAgent.getNearChatRadius();
            //if (!avatarp || dist_vec_squared(avatarp->getPositionAgent(), gAgent.getPositionAgent()) > say_distance_squared)
            F32 dist_near_chat = (F32)LFSimFeatureHandler::getInstance()->sayRange();
// </FS:CR> Opensim
            if (distance_squared > dist_near_chat * dist_near_chat)
            {
                // diminish far-off chat
                // <FS:Ansariel> FIRE-3572: Customize local chat color brightness change based on distance
                //r_color_alpha = 0.8f;
                static LLCachedControl<F32> fsBeyondNearbyChatColorDiminishFactor(gSavedSettings, "FSBeyondNearbyChatColorDiminishFactor", 0.8f);
                r_color_alpha = fsBeyondNearbyChatColorDiminishFactor();
                // </FS:Ansariel>
            }
            else
            {
                r_color_alpha = 1.0f;
            }
        }
    }
}


//static
void LLViewerChat::getChatColor(const LLChat& chat, std::string& r_color_name, F32& r_color_alpha)
{
    if(chat.mMuted)
    {
        r_color_name = "LtGray";
    }
    else
    {
        switch(chat.mSourceType)
        {
            case CHAT_SOURCE_SYSTEM:
                r_color_name = "SystemChatColor";
                break;

            case CHAT_SOURCE_AGENT:
                if (chat.mFromID.isNull())
                {
                    r_color_name = "SystemChatColor";
                }
                else
                {
                    if(gAgentID == chat.mFromID)
                    {
                        r_color_name = "UserChatColor";
                    }
                    else
                    {
                        r_color_name = "AgentChatColor";
                    }
                }
                break;

            case CHAT_SOURCE_OBJECT:
                if (chat.mChatType == CHAT_TYPE_DEBUG_MSG)
                {
                    r_color_name = "ScriptErrorColor";
                }
                else if ( chat.mChatType == CHAT_TYPE_OWNER )
                {
                    r_color_name = "llOwnerSayChatColor";
                }
                else if ( chat.mChatType == CHAT_TYPE_DIRECT )
                {
                    r_color_name = "DirectChatColor";
                }
                else if ( chat.mChatType == CHAT_TYPE_IM )
                {
                    r_color_name = "ObjectIMColor";
                }
                else
                {
                    r_color_name = "ObjectChatColor";
                }
                break;
            default:
                r_color_name = "White";
        }

        if (!chat.mPosAgent.isExactlyZero())
        {
            LLVector3 pos_agent = gAgent.getPositionAgent();
            F32 distance_squared = dist_vec_squared(pos_agent, chat.mPosAgent);
// <FS:CR> Opensim
            //F32 dist_near_chat = gAgent.getNearChatRadius();
            F32 dist_near_chat = (F32)LFSimFeatureHandler::getInstance()->sayRange();
// </FS:CR> Opensim
            if (distance_squared > dist_near_chat * dist_near_chat)
            {
                // diminish far-off chat
                // <FS:Ansariel> FIRE-3572: Customize local chat color brightness change based on distance
                //r_color_alpha = 0.8f;
                static LLCachedControl<F32> fsBeyondNearbyChatColorDiminishFactor(gSavedSettings, "FSBeyondNearbyChatColorDiminishFactor", 0.8f);
                r_color_alpha = fsBeyondNearbyChatColorDiminishFactor();
                // </FS:Ansariel>
            }
            else
            {
                r_color_alpha = 1.0f;
            }
        }
    }

}


//static
LLFontGL* LLViewerChat::getChatFont()
{
    S32 font_size = gSavedSettings.getS32("ChatFontSize");
    LLFontGL* fontp = NULL;
    switch(font_size)
    {
        case 0:
            fontp = LLFontGL::getFontSansSerifSmall();
            break;
        default:
        case 1:
            fontp = LLFontGL::getFontSansSerif();
            break;
        case 2:
            fontp = LLFontGL::getFontSansSerifBig();
            break;
        case 3:
            fontp = LLFontGL::getFontSansSerifHuge();
            break;
    }

    return fontp;

}

//static
S32 LLViewerChat::getChatFontSize()
{
    return gSavedSettings.getS32("ChatFontSize");
}


//static
void LLViewerChat::formatChatMsg(const LLChat& chat, std::string& formated_msg)
{
    std::string tmpmsg = chat.mText;

    // show @name instead of slurl for chat mentions
    LLUrlMatch match;
    while (LLUrlRegistry::instance().findUrl(tmpmsg, match, LLUrlRegistryNullCallback, false, true))
    {
        tmpmsg.replace(match.getStart(), match.getEnd() - match.getStart() + 1, match.getLabel());
    }

    if(chat.mChatStyle == CHAT_STYLE_IRC)
    {
        formated_msg = chat.mFromName + tmpmsg.substr(3);
    }
    else
    {
        formated_msg = tmpmsg;
    }

}

//static
std::string LLViewerChat::getSenderSLURL(const LLChat& chat, const LLSD& args)
{
    switch (chat.mSourceType)
    {
    case CHAT_SOURCE_AGENT:
        return LLSLURL("agent", chat.mFromID, "about").getSLURLString();

    case CHAT_SOURCE_OBJECT:
        return getObjectImSLURL(chat, args);

    // <FS:Ansariel> Stop spamming the log when processing system messages
    case CHAT_SOURCE_SYSTEM:
        return LLStringUtil::null;
    // </FS:Ansariel>

    default:
        LL_WARNS() << "Getting SLURL for an unsupported sender type: " << chat.mSourceType << LL_ENDL;
    }

    return LLStringUtil::null;
}

//static
std::string LLViewerChat::getObjectImSLURL(const LLChat& chat, const LLSD& args)
{
    std::string url = LLSLURL("objectim", chat.mFromID, "").getSLURLString();
    // <FS:Ansariel> FIRE-6238: objectim SLURLs don't get handled correctly if the object's name contains ampersands
    //url += "?name=" + chat.mFromName;
    url += "?name=" + LLURI::escape(chat.mFromName);
    // </FS:Ansariel>
    url += "&owner=" + chat.mOwnerID.asString();

    std::string slurl = args["slurl"].asString();
    if (slurl.empty())
    {
        LLViewerRegion *region = LLWorld::getInstance()->getRegionFromPosAgent(chat.mPosAgent);
        if(region)
        {
            LLSLURL region_slurl(region->getName(), chat.mPosAgent);
            slurl = region_slurl.getLocationString();
        }
    }

    url += "&slurl=" + LLURI::escape(slurl);

    return url;
}

//static
boost::signals2::connection LLViewerChat::setFontChangedCallback(const font_change_signal_t::slot_type& cb)
{
    return sChatFontChangedSignal.connect(cb);
}

//static
void LLViewerChat::signalChatFontChanged()
{
    // Notify all observers that our font has changed
    sChatFontChangedSignal(getChatFont());
}
