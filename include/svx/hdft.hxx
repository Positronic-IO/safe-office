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
#ifndef INCLUDED_SVX_HDFT_HXX
#define INCLUDED_SVX_HDFT_HXX

#include <sfx2/tabdlg.hxx>

#include <vcl/fixed.hxx>
#include <vcl/field.hxx>
#include <vcl/group.hxx>
#include <vcl/svapp.hxx>
#include <vcl/weld.hxx>

#include <svx/pagectrl.hxx>
#include <svx/svxdllapi.h>

namespace svx
{
    SVX_DLLPUBLIC bool ShowBorderBackgroundDlg( vcl::Window* pParent, SfxItemSet* pBBSet );
}

class SVX_DLLPUBLIC SvxHFPage : public SfxTabPage
{
    using TabPage::ActivatePage;
    using TabPage::DeactivatePage;

public:

    virtual bool    FillItemSet( SfxItemSet* rOutSet ) override;
    virtual void    Reset( const SfxItemSet* rSet ) override;

    virtual         ~SvxHFPage() override;
    virtual void    dispose() override;

    void DisableDeleteQueryBox() { mbDisableQueryBox = true; }

    virtual void PageCreated(const SfxAllItemSet&) override;

    void            EnableDynamicSpacing();

protected:
    static const sal_uInt16 pRanges[];

    virtual void    ActivatePage( const SfxItemSet& rSet ) override;
    virtual DeactivateRC   DeactivatePage( SfxItemSet* pSet ) override;

    SvxHFPage( vcl::Window* pParent, const SfxItemSet& rSet, sal_uInt16 nSetId );

    VclPtr<FixedText>       m_pPageLbl;
    VclPtr<CheckBox>        m_pTurnOnBox;
    VclPtr<CheckBox>        m_pCntSharedBox;
    VclPtr<CheckBox>        m_pCntSharedFirstBox;
    VclPtr<FixedText>       m_pLMLbl;
    VclPtr<MetricField>     m_pLMEdit;
    VclPtr<FixedText>       m_pRMLbl;
    VclPtr<MetricField>     m_pRMEdit;
    VclPtr<FixedText>       m_pDistFT;
    VclPtr<MetricField>     m_pDistEdit;
    VclPtr<CheckBox>        m_pDynSpacingCB;
    VclPtr<FixedText>       m_pHeightFT;
    VclPtr<MetricField>     m_pHeightEdit;
    VclPtr<CheckBox>        m_pHeightDynBtn;
    VclPtr<SvxPageWindow>   m_pBspWin;
    VclPtr<PushButton>      m_pBackgroundBtn;

    sal_uInt16       nId;
    SfxItemSet*      pBBSet;
    bool            mbDisableQueryBox : 1;
    bool            mbEnableDrawingLayerFillStyles : 1;

    void            InitHandler();
    DECL_LINK(TurnOnHdl, Button*, void);
    DECL_LINK(DistModify, Edit&, void);
    DECL_LINK(HeightModify, Edit&, void);
    DECL_LINK(BorderModify, Edit&, void);
    DECL_LINK(BackgroundHdl, Button*, void);
    DECL_LINK(RangeFocusHdl, Control&, void);
    void RangeHdl();
    void            UpdateExample();

private:
    SVX_DLLPRIVATE void         ResetBackground_Impl( const SfxItemSet& rSet );
};

class SVX_DLLPUBLIC SvxHeaderPage : public SvxHFPage
{
public:
    static VclPtr<SfxTabPage> Create( TabPageParent pParent, const SfxItemSet* rSet );
    // returns the Which values to the range
    static const sal_uInt16*  GetRanges() { return pRanges; }
    SVX_DLLPRIVATE SvxHeaderPage( vcl::Window* pParent, const SfxItemSet& rSet );
};

class SVX_DLLPUBLIC SvxFooterPage : public SvxHFPage
{
public:
    static VclPtr<SfxTabPage> Create( TabPageParent pParent, const SfxItemSet* rSet );
    static const sal_uInt16*  GetRanges() { return pRanges; }
    SVX_DLLPRIVATE SvxFooterPage(   vcl::Window* pParent, const SfxItemSet& rSet );
};

class SVX_DLLPUBLIC DeleteHeaderDialog : public weld::MessageDialogController
{
public:
    DeleteHeaderDialog(weld::Widget* pParent)
        : MessageDialogController(pParent, "svx/ui/deleteheaderdialog.ui",
                "DeleteHeaderDialog")
    {
    }
};

class SVX_DLLPUBLIC DeleteFooterDialog : public weld::MessageDialogController
{
public:
    DeleteFooterDialog(weld::Widget* pParent)
        : MessageDialogController(pParent, "svx/ui/deletefooterdialog.ui",
                "DeleteFooterDialog")
    {
    }
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
