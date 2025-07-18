# -*- cmake -*-

project(llui)

include(00-Common)
include(Hunspell)
include(LLCommon)
include(LLImage)
include(LLCoreHttp)
include(LLWindow)
include(LLCoreHttp)

set(llui_SOURCE_FILES
    fsregistrarutils.cpp

    llaccordionctrl.cpp
    llaccordionctrltab.cpp
    llbadge.cpp
    llbadgeholder.cpp
    llbadgeowner.cpp
    llbutton.cpp
    llchatentry.cpp
    llchatmentionhelper.cpp
    llcheckboxctrl.cpp
    llclipboard.cpp
    llcombobox.cpp
    llcommandmanager.cpp
    llconsole.cpp
    llcontainerview.cpp
    llctrlselectioninterface.cpp
    lldockablefloater.cpp
    lldockcontrol.cpp
    lldraghandle.cpp
    lleditmenuhandler.cpp
    llemojidictionary.cpp
    llemojihelper.cpp
    llf32uictrl.cpp
    llfiltereditor.cpp
    llflashtimer.cpp
    llflatlistview.cpp
    llfloater.cpp
    llfloaterreg.cpp
    llfloaterreglistener.cpp
    llflyoutbutton.cpp 
    llfocusmgr.cpp
    llfolderview.cpp
    llfolderviewitem.cpp
    llfolderviewmodel.cpp
    lliconctrl.cpp
    llkeywords.cpp
    lllayoutstack.cpp
    lllineeditor.cpp
    llloadingindicator.cpp
    lllocalcliprect.cpp
    llmenubutton.cpp
    llmenugl.cpp
    llmodaldialog.cpp
    llmultifloater.cpp 
    llmultislider.cpp
    llmultisliderctrl.cpp
    llnotifications.cpp
    llnotificationslistener.cpp
    llnotificationsutil.cpp
    llpanel.cpp
    llprogressbar.cpp
    llradiogroup.cpp
    llresizebar.cpp
    llresizehandle.cpp
    llresmgr.cpp
    llrngwriter.cpp
    llscrollbar.cpp
    llscrollcontainer.cpp
    llscrollingpanellist.cpp
    llscrolllistcell.cpp
    llscrolllistcolumn.cpp
    llscrolllistctrl.cpp
    llscrolllistitem.cpp
    llsearcheditor.cpp
    llslider.cpp
    llsliderctrl.cpp
    llspellcheck.cpp
    llspinctrl.cpp
    llstatbar.cpp
    llstatgraph.cpp
    llstatview.cpp
    llstyle.cpp
    lltabcontainer.cpp
    lltextbase.cpp
    lltextbox.cpp
    lltexteditor.cpp
    lltextparser.cpp
    lltextutil.cpp
    lltextvalidate.cpp
    lltimectrl.cpp
    lltrans.cpp
    lltransutil.cpp
    lltoggleablemenu.cpp
    lltoolbar.cpp
    lltooltip.cpp
    llui.cpp
    lluicolor.cpp
    lluicolortable.cpp
    lluictrl.cpp
    lluictrlfactory.cpp
    lluistring.cpp
    lluiusage.cpp
    llundo.cpp
    llurlaction.cpp
    llurlentry.cpp
    llurlmatch.cpp
    llurlregistry.cpp
    llviewborder.cpp
    llviewinject.cpp
    llviewmodel.cpp
    llview.cpp
    llviewquery.cpp
    llviewereventrecorder.cpp
    llvirtualtrackball.cpp
    llwindowshade.cpp
    llxuiparser.cpp
    llxyvector.cpp
    )
    
set(llui_HEADER_FILES
    CMakeLists.txt

    fsregistrarutils.h

    llaccordionctrl.h
    llaccordionctrltab.h
    llbadge.h
    llbadgeholder.h
    llbadgeowner.h
    llbutton.h
    llcallbackmap.h
    llchatentry.h
    llchat.h
    llchatmentionhelper.h
    llcheckboxctrl.h
    llclipboard.h
    llcombobox.h
    llcommandmanager.h
    llconsole.h
    llcontainerview.h
    llctrlselectioninterface.h
    lldraghandle.h
    lldockablefloater.h
    lldockcontrol.h
    lleditmenuhandler.h
    llemojidictionary.h
    llemojihelper.h
    llf32uictrl.h
    llfiltereditor.h 
    llflashtimer.h
    llflatlistview.h
    llfloater.h
    llfloaterreg.h
    llfloaterreglistener.h
    llflyoutbutton.h 
    llfocusmgr.h
    llfolderview.h
    llfolderviewitem.h
    llfolderviewmodel.h
    llfunctorregistry.h
    llhelp.h
    lliconctrl.h
    llkeywords.h
    lllayoutstack.h
    lllazyvalue.h
    lllineeditor.h
    llloadingindicator.h
    lllocalcliprect.h
    llmenubutton.h
    llmenugl.h
    llmodaldialog.h
    llmultifloater.h 
    llmultisliderctrl.h
    llmultislider.h
    llnotificationptr.h
    llnotifications.h
    llnotificationslistener.h
    llnotificationsutil.h
    llnotificationtemplate.h
    llnotificationvisibilityrule.h
    llpanel.h
    llprogressbar.h
    llradiogroup.h
    llresizebar.h
    llresizehandle.h
    llresmgr.h
    llrngwriter.h
    llsearchablecontrol.h
    llsearcheditor.h 
    llscrollbar.h
    llscrollcontainer.h
    llscrollingpanellist.h
    llscrolllistcell.h
    llscrolllistcolumn.h
    llscrolllistctrl.h
    llscrolllistitem.h
    llsliderctrl.h
    llslider.h
    llspellcheck.h
    llspellcheckmenuhandler.h
    llspinctrl.h
    llstatbar.h
    llstatgraph.h
    llstatview.h
    llstyle.h
    lltabcontainer.h
    lltextbase.h
    lltextbox.h
    lltexteditor.h
    lltextparser.h
    lltextutil.h
    lltextvalidate.h
    lltimectrl.h
    lltoggleablemenu.h
    lltoolbar.h
    lltooltip.h
    lltrans.h
    lltransutil.h
    lluicolortable.h
    lluiconstants.h
    lluictrlfactory.h
    lluictrl.h
    lluifwd.h
    llui.h
    lluicolor.h
    lluistring.h
    lluiusage.h
    llundo.h
    llurlaction.h
    llurlentry.h
    llurlmatch.h
    llurlregistry.h
    llviewborder.h
    llviewinject.h
    llviewmodel.h
    llview.h
    llviewereventrecorder.h
    llviewquery.h
    llvirtualtrackball.h
    llwindowshade.h
    llxuiparser.h
    llxyvector.h
    )

SET(llurlentry_TEST_DEPENDENCIES
    llurlmatch.cpp
    llurlregistry.cpp
    )

set_source_files_properties(llurlentry.cpp
    PROPERTIES LL_TEST_ADDITIONAL_SOURCE_FILES
    "${llurlentry_TEST_DEPENDENCIES}"
    )

list(APPEND llui_SOURCE_FILES ${llui_HEADER_FILES})

add_library (llui ${llui_SOURCE_FILES})
target_include_directories( llui  INTERFACE   ${CMAKE_CURRENT_SOURCE_DIR})
# Libraries on which this library depends, needed for Linux builds
# Sort by high-level to low-level
target_link_libraries(llui
        llrender
        llwindow
        llimage
        llinventory
        llmessage
        llcorehttp
        llfilesystem
        llxml
        llmath
        ll::hunspell
        llcommon
    )

# Add tests
if(LL_TESTS)
  include(LLAddBuildTest)
  set(test_libs llmessage llcorehttp llxml llrender llcommon ll::hunspell)

  SET(llui_TEST_SOURCE_FILES
      llurlmatch.cpp
      )
  set_property( SOURCE ${llui_TEST_SOURCE_FILES} PROPERTY LL_TEST_ADDITIONAL_LIBRARIES ${test_libs})
  LL_ADD_PROJECT_UNIT_TESTS(llui "${llui_TEST_SOURCE_FILES}")
  # INTEGRATION TESTS

  if(NOT LINUX)
    set(test_libs llui llmessage llcorehttp llxml llrender llcommon ll::hunspell )
    LL_ADD_INTEGRATION_TEST(llurlentry llurlentry.cpp "${test_libs}")
  endif(NOT LINUX)
endif(LL_TESTS)
