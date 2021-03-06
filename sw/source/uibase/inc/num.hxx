/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */
#ifndef INCLUDED_SW_SOURCE_UIBASE_INC_NUM_HXX
#define INCLUDED_SW_SOURCE_UIBASE_INC_NUM_HXX

#include <sfx2/tabdlg.hxx>
#include <vcl/menubtn.hxx>
#include <vcl/field.hxx>
#include <vcl/fixed.hxx>
#include <vcl/layout.hxx>
#include <vcl/button.hxx>
#include <vcl/lstbox.hxx>
#include <vcl/edit.hxx>
#include <svx/stddlg.hxx>
#include "numprevw.hxx"
#include <numrule.hxx>

class SwWrtShell;
class SvxBrushItem;
class SwOutlineTabDialog;

class SwNumPositionTabPage : public SfxTabPage
{
    SwNumRule*          pActNum;
    SwNumRule*          pSaveNum;
    SwWrtShell*         pWrtSh;

    SwOutlineTabDialog* pOutlineDlg;
    sal_uInt16          nActNumLvl;

    bool                bModified           : 1;
    bool                bPreset             : 1;
    bool                bInInintControl     : 1;  // work around modify-error; should be resolved from 391 on
    bool                bLabelAlignmentPosAndSpaceModeActive;

    std::unique_ptr<weld::TreeView> m_xLevelLB;
    std::unique_ptr<weld::Widget> m_xPositionFrame;

    // former set of controls shown for numbering rules containing list level
    // attributes in SvxNumberFormat::SvxNumPositionAndSpaceMode == LABEL_WIDTH_AND_POSITION
    std::unique_ptr<weld::Label> m_xDistBorderFT;
    std::unique_ptr<weld::MetricSpinButton> m_xDistBorderMF;
    std::unique_ptr<weld::CheckButton> m_xRelativeCB;
    std::unique_ptr<weld::Label> m_xIndentFT;
    std::unique_ptr<weld::MetricSpinButton> m_xIndentMF;
    std::unique_ptr<weld::Label> m_xDistNumFT;
    std::unique_ptr<weld::MetricSpinButton> m_xDistNumMF;
    std::unique_ptr<weld::Label> m_xAlignFT;
    std::unique_ptr<weld::ComboBoxText> m_xAlignLB;

    // new set of controls shown for numbering rules containing list level
    // attributes in SvxNumberFormat::SvxNumPositionAndSpaceMode == LABEL_ALIGNMENT
    std::unique_ptr<weld::Label> m_xLabelFollowedByFT;
    std::unique_ptr<weld::ComboBoxText> m_xLabelFollowedByLB;
    std::unique_ptr<weld::Label> m_xListtabFT;
    std::unique_ptr<weld::MetricSpinButton> m_xListtabMF;
    std::unique_ptr<weld::Label> m_xAlign2FT;
    std::unique_ptr<weld::ComboBoxText> m_xAlign2LB;
    std::unique_ptr<weld::Label> m_xAlignedAtFT;
    std::unique_ptr<weld::MetricSpinButton> m_xAlignedAtMF;
    std::unique_ptr<weld::Label> m_xIndentAtFT;
    std::unique_ptr<weld::MetricSpinButton> m_xIndentAtMF;
    std::unique_ptr<weld::Button> m_xStandardPB;
    std::unique_ptr<SwNumberingPreview> m_xPreviewWIN;


    void                InitControls();

    DECL_LINK(LevelHdl, weld::TreeView&, void);
    DECL_LINK(EditModifyHdl, weld::ComboBoxText&, void);
    DECL_LINK(DistanceHdl, weld::MetricSpinButton&, void);
    DECL_LINK(RelativeHdl, weld::ToggleButton&, void);
    DECL_LINK(StandardHdl, weld::Button&, void);

    void InitPosAndSpaceMode();
    void ShowControlsDependingOnPosAndSpaceMode();

    DECL_LINK(LabelFollowedByHdl_Impl, weld::ComboBoxText&, void);
    DECL_LINK(ListtabPosHdl_Impl, weld::MetricSpinButton&, void);
    DECL_LINK(AlignAtHdl_Impl, weld::MetricSpinButton&, void);
    DECL_LINK(IndentAtHdl_Impl, weld::MetricSpinButton&, void);

    using SfxTabPage::ActivatePage;
    using SfxTabPage::DeactivatePage;

public:

    SwNumPositionTabPage(TabPageParent pParent, const SfxItemSet& rSet);
    virtual ~SwNumPositionTabPage() override;
    virtual void        dispose() override;

    virtual void        ActivatePage(const SfxItemSet& rSet) override;
    virtual DeactivateRC   DeactivatePage(SfxItemSet *pSet) override;
    virtual bool        FillItemSet( SfxItemSet* rSet ) override;
    virtual void        Reset( const SfxItemSet* rSet ) override;

    static VclPtr<SfxTabPage> Create( TabPageParent pParent,
                                      const SfxItemSet* rAttrSet);

    void                SetOutlineTabDialog(SwOutlineTabDialog* pDlg){pOutlineDlg = pDlg;}
    void                SetWrtShell(SwWrtShell* pSh);
#ifdef DBG_UTIL
    void                SetModified();
#else
    void                SetModified()
                            {   bModified = true;
                                m_xPreviewWIN->SetLevel(nActNumLvl);
                                m_xPreviewWIN->queue_draw();
                            }
#endif
};

class SwSvxNumBulletTabDialog final : public SfxTabDialog
{
    SwWrtShell&         rWrtSh;
    sal_uInt16 m_nSingleNumPageId;
    sal_uInt16 m_nBulletPageId;
    sal_uInt16 m_nOptionsPageId;
    sal_uInt16 m_nPositionPageId;

    virtual short   Ok() override;
    virtual void    PageCreated(sal_uInt16 nPageId, SfxTabPage& rPage) override;
    DECL_LINK(RemoveNumberingHdl, Button*, void);

public:
    SwSvxNumBulletTabDialog(vcl::Window* pParent,
                    const SfxItemSet* pSwItemSet,
                    SwWrtShell &);
    virtual ~SwSvxNumBulletTabDialog() override;
};
#endif // INCLUDED_SW_SOURCE_UIBASE_INC_NUM_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
