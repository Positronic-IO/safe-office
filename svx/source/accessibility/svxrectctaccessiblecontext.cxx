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

#include <svxrectctaccessiblecontext.hxx>
#include <com/sun/star/accessibility/AccessibleRole.hpp>
#include <com/sun/star/accessibility/AccessibleEventId.hpp>
#include <unotools/accessiblestatesethelper.hxx>
#include <com/sun/star/accessibility/AccessibleStateType.hpp>
#include <com/sun/star/beans/PropertyChangeEvent.hpp>
#include <com/sun/star/awt/XWindow.hpp>
#include <cppuhelper/typeprovider.hxx>
#include <cppuhelper/supportsservice.hxx>
#include <toolkit/helper/convert.hxx>
#include <vcl/svapp.hxx>
#include <osl/mutex.hxx>
#include <tools/debug.hxx>
#include <tools/gen.hxx>

#include <svx/strings.hrc>
#include <svx/dlgctrl.hxx>
#include <svx/dialmgr.hxx>
#include <comphelper/accessibleeventnotifier.hxx>
#include <com/sun/star/accessibility/AccessibleRelationType.hpp>
#include <unotools/accessiblerelationsethelper.hxx>

using namespace ::cppu;
using namespace ::osl;
using namespace ::com::sun::star;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::accessibility;

using namespace ::com::sun::star::lang;

#define MAX_NUM_OF_CHILDREN   9
#define NOCHILDSELECTED     -1

// internal
namespace
{
    struct ChildIndexToPointData
    {
        const char* pResIdName;
        const char* pResIdDescr;
        RectPoint  ePoint;
    };
}


static const ChildIndexToPointData* IndexToPoint( long nIndex )
{
    DBG_ASSERT( nIndex < 9 && nIndex >= 0, "-IndexToPoint(): invalid child index! You have been warned..." );

    // corners are counted from left to right and top to bottom
    static const ChildIndexToPointData  pCornerData[] =
    {                                                                   // index
        {   RID_SVXSTR_RECTCTL_ACC_CHLD_LT, RID_SVXSTR_RECTCTL_ACC_CHLD_LT, RectPoint::LT },    //  0
        {   RID_SVXSTR_RECTCTL_ACC_CHLD_MT, RID_SVXSTR_RECTCTL_ACC_CHLD_MT, RectPoint::MT },    //  1
        {   RID_SVXSTR_RECTCTL_ACC_CHLD_RT, RID_SVXSTR_RECTCTL_ACC_CHLD_RT, RectPoint::RT },    //  2
        {   RID_SVXSTR_RECTCTL_ACC_CHLD_LM, RID_SVXSTR_RECTCTL_ACC_CHLD_LM, RectPoint::LM },    //  3
        {   RID_SVXSTR_RECTCTL_ACC_CHLD_MM, RID_SVXSTR_RECTCTL_ACC_CHLD_MM, RectPoint::MM },    //  4
        {   RID_SVXSTR_RECTCTL_ACC_CHLD_RM, RID_SVXSTR_RECTCTL_ACC_CHLD_RM, RectPoint::RM },    //  5
        {   RID_SVXSTR_RECTCTL_ACC_CHLD_LB, RID_SVXSTR_RECTCTL_ACC_CHLD_LB, RectPoint::LB },    //  6
        {   RID_SVXSTR_RECTCTL_ACC_CHLD_MB, RID_SVXSTR_RECTCTL_ACC_CHLD_MB, RectPoint::MB },    //  7
        {   RID_SVXSTR_RECTCTL_ACC_CHLD_RB, RID_SVXSTR_RECTCTL_ACC_CHLD_RB, RectPoint::RB }     //  8
    };

    return pCornerData + nIndex;
}


static long PointToIndex( RectPoint ePoint )
{
    long    nRet( static_cast<long>(ePoint) );
    // corner control
    // corners are counted from left to right and top to bottom
    DBG_ASSERT( int(RectPoint::LT) == 0 && int(RectPoint::MT) == 1 && int(RectPoint::RT) == 2 && int(RectPoint::LM) == 3 && int(RectPoint::MM) == 4 && int(RectPoint::RM) == 5 &&
                int(RectPoint::LB) == 6 && int(RectPoint::MB) == 7 && int(RectPoint::RB) == 8, "*PointToIndex(): unexpected enum value!" );

    nRet = static_cast<long>(ePoint);

    return nRet;
}

SvxRectCtlAccessibleContext::SvxRectCtlAccessibleContext(
    const Reference< XAccessible >&     rxParent,
    SvxRectCtl&                         rRepr ) :

    SvxRectCtlAccessibleContext_Base( m_aMutex ),
    mxParent( rxParent ),
    mpRepr( &rRepr ),
    mnClientId( 0 ),
    mnSelectedChild( NOCHILDSELECTED )
{
    {
        ::SolarMutexGuard aSolarGuard;
        msName = SvxResId( RID_SVXSTR_RECTCTL_ACC_CORN_NAME );
        msDescription = SvxResId( RID_SVXSTR_RECTCTL_ACC_CORN_DESCR );
    }

    mvChildren.resize(MAX_NUM_OF_CHILDREN);
}


SvxRectCtlAccessibleContext::~SvxRectCtlAccessibleContext()
{

    if( IsAlive() )
    {
        osl_atomic_increment( &m_refCount );
        dispose();      // set mpRepr = NULL & release all children
    }
}

// XAccessible
Reference< XAccessibleContext > SAL_CALL SvxRectCtlAccessibleContext::getAccessibleContext()
{
    return this;
}

// XAccessibleComponent
sal_Bool SAL_CALL SvxRectCtlAccessibleContext::containsPoint( const awt::Point& rPoint )
{
    // no guard -> done in getBounds()
//  return GetBoundingBox().IsInside( VCLPoint( rPoint ) );
    return tools::Rectangle( Point( 0, 0 ), GetBoundingBox().GetSize() ).IsInside( VCLPoint( rPoint ) );
}

Reference< XAccessible > SAL_CALL SvxRectCtlAccessibleContext::getAccessibleAtPoint( const awt::Point& rPoint )
{
    ::osl::MutexGuard           aGuard( m_aMutex );

    ThrowExceptionIfNotAlive();

    Reference< XAccessible >    xRet;

    long                        nChild = PointToIndex( mpRepr->GetApproxRPFromPixPt( rPoint ) );

    if( nChild != NOCHILDSELECTED )
        xRet = getAccessibleChild( nChild );

    return xRet;
}

awt::Rectangle SAL_CALL SvxRectCtlAccessibleContext::getBounds()
{
    // no guard -> done in GetBoundingBox()
    return AWTRectangle( GetBoundingBox() );
}

awt::Point SAL_CALL SvxRectCtlAccessibleContext::getLocation()
{
    // no guard -> done in GetBoundingBox()
    return AWTPoint( GetBoundingBox().TopLeft() );
}

awt::Point SAL_CALL SvxRectCtlAccessibleContext::getLocationOnScreen()
{
    // no guard -> done in GetBoundingBoxOnScreen()
    return AWTPoint( GetBoundingBoxOnScreen().TopLeft() );
}

awt::Size SAL_CALL SvxRectCtlAccessibleContext::getSize()
{
    // no guard -> done in GetBoundingBox()
    return AWTSize( GetBoundingBox().GetSize() );
}

bool SvxRectCtlAccessibleContext::isVisible()
{
    ::osl::MutexGuard           aGuard( m_aMutex );

    ThrowExceptionIfNotAlive();

    return mpRepr->IsVisible();
}

// XAccessibleContext
sal_Int32 SAL_CALL SvxRectCtlAccessibleContext::getAccessibleChildCount()
{
    ::osl::MutexGuard   aGuard( m_aMutex );

    ThrowExceptionIfNotAlive();

    return SvxRectCtl::NO_CHILDREN;
}

Reference< XAccessible > SAL_CALL SvxRectCtlAccessibleContext::getAccessibleChild( sal_Int32 nIndex )
{
    checkChildIndex( nIndex );

    Reference< XAccessible > xChild(mvChildren[ nIndex ].get());
    if( !xChild.is() )
    {
        ::SolarMutexGuard aSolarGuard;

        ::osl::MutexGuard   aGuard( m_aMutex );

        ThrowExceptionIfNotAlive();

        xChild = mvChildren[ nIndex ].get();

        if( !xChild.is() )
        {
            const ChildIndexToPointData*    p = IndexToPoint( nIndex );
            OUString aName(SvxResId(p->pResIdName));
            OUString aDescr(SvxResId(p->pResIdDescr));

            tools::Rectangle       aFocusRect( mpRepr->CalculateFocusRectangle( p->ePoint ) );

            SvxRectCtlChildAccessibleContext*   pChild = new SvxRectCtlChildAccessibleContext(
                                                    this, *mpRepr, aName, aDescr, aFocusRect, nIndex );
            mvChildren[ nIndex ] = pChild;
            xChild = pChild;

            // set actual state
            if( mnSelectedChild == nIndex )
                pChild->setStateChecked( true );
        }
    }

    return xChild;
}

Reference< XAccessible > SAL_CALL SvxRectCtlAccessibleContext::getAccessibleParent()
{
    return mxParent;
}

sal_Int32 SAL_CALL SvxRectCtlAccessibleContext::getAccessibleIndexInParent()
{
    ::osl::MutexGuard   aGuard( m_aMutex );
    //  Use a simple but slow solution for now.  Optimize later.

    //  Iterate over all the parent's children and search for this object.
    if( mxParent.is() )
    {
        Reference< XAccessibleContext >     xParentContext( mxParent->getAccessibleContext() );
        if( xParentContext.is() )
        {
            sal_Int32                       nChildCount = xParentContext->getAccessibleChildCount();
            for( sal_Int32 i = 0 ; i < nChildCount ; ++i )
            {
                Reference< XAccessible >    xChild( xParentContext->getAccessibleChild( i ) );
                if( xChild.get() == static_cast<XAccessible*>(this) )
                    return i;
            }
        }
   }

   //   Return -1 to indicate that this object's parent does not know about the
   //   object.
   return -1;
}

sal_Int16 SAL_CALL SvxRectCtlAccessibleContext::getAccessibleRole()
{
    return AccessibleRole::PANEL;
}

OUString SAL_CALL SvxRectCtlAccessibleContext::getAccessibleDescription()
{
    ::osl::MutexGuard   aGuard( m_aMutex );
    return msDescription + " Please use arrow key to selection.";
}

OUString SAL_CALL SvxRectCtlAccessibleContext::getAccessibleName()
{
    ::osl::MutexGuard   aGuard( m_aMutex );
    return msName;
}

/** Return empty reference to indicate that the relation set is not
    supported.
*/
Reference< XAccessibleRelationSet > SAL_CALL SvxRectCtlAccessibleContext::getAccessibleRelationSet()
{
    //return Reference< XAccessibleRelationSet >();
    utl::AccessibleRelationSetHelper* pRelationSetHelper = new utl::AccessibleRelationSetHelper;
    uno::Reference< css::accessibility::XAccessibleRelationSet > xSet = pRelationSetHelper;
    vcl::Window* pWindow = mpRepr;
    if ( pWindow )
    {
        // vcl::Window *pLabeledBy = pWindow->GetAccRelationLabeledBy();
        vcl::Window *pLabeledBy = pWindow->GetAccessibleRelationLabeledBy();
        if ( pLabeledBy && pLabeledBy != pWindow )
        {
            uno::Sequence< uno::Reference< uno::XInterface > > aSequence { pLabeledBy->GetAccessible() };
            pRelationSetHelper->AddRelation( css::accessibility::AccessibleRelation( css::accessibility::AccessibleRelationType::LABELED_BY, aSequence ) );
        }
        vcl::Window* pMemberOf = pWindow->GetAccessibleRelationMemberOf();
        if ( pMemberOf && pMemberOf != pWindow )
        {
            uno::Sequence< uno::Reference< uno::XInterface > > aSequence { pMemberOf->GetAccessible() };
            pRelationSetHelper->AddRelation( css::accessibility::AccessibleRelation( css::accessibility::AccessibleRelationType::MEMBER_OF, aSequence ) );
        }
    }
    return xSet;
}

Reference< XAccessibleStateSet > SAL_CALL SvxRectCtlAccessibleContext::getAccessibleStateSet()
{
    ::osl::MutexGuard                       aGuard( m_aMutex );
    utl::AccessibleStateSetHelper*          pStateSetHelper = new utl::AccessibleStateSetHelper;

    if( IsAlive() )
    {
        pStateSetHelper->AddState( AccessibleStateType::ENABLED );
        pStateSetHelper->AddState( AccessibleStateType::FOCUSABLE );
        if( mpRepr->HasFocus() )
            pStateSetHelper->AddState( AccessibleStateType::FOCUSED );
        pStateSetHelper->AddState( AccessibleStateType::OPAQUE );

        pStateSetHelper->AddState( AccessibleStateType::SHOWING );

        if( isVisible() )
            pStateSetHelper->AddState( AccessibleStateType::VISIBLE );
    }
    else
        pStateSetHelper->AddState( AccessibleStateType::DEFUNC );

    return pStateSetHelper;
}

lang::Locale SAL_CALL SvxRectCtlAccessibleContext::getLocale()
{
    ::osl::MutexGuard                           aGuard( m_aMutex );
    if( mxParent.is() )
    {
        Reference< XAccessibleContext > xParentContext( mxParent->getAccessibleContext() );
        if( xParentContext.is() )
            return xParentContext->getLocale();
    }

    //  No parent.  Therefore throw exception to indicate this cluelessness.
    throw IllegalAccessibleComponentStateException();
}

void SAL_CALL SvxRectCtlAccessibleContext::addAccessibleEventListener( const Reference< XAccessibleEventListener >& xListener )
{
    if (xListener.is())
    {
        ::osl::MutexGuard   aGuard( m_aMutex );
        if (!mnClientId)
            mnClientId = comphelper::AccessibleEventNotifier::registerClient( );
        comphelper::AccessibleEventNotifier::addEventListener( mnClientId, xListener );
    }
}

void SAL_CALL SvxRectCtlAccessibleContext::removeAccessibleEventListener( const Reference< XAccessibleEventListener >& xListener )
{
    if (xListener.is())
    {
        ::osl::MutexGuard   aGuard( m_aMutex );

        sal_Int32 nListenerCount = comphelper::AccessibleEventNotifier::removeEventListener( mnClientId, xListener );
        if ( !nListenerCount )
        {
            // no listeners anymore
            // -> revoke ourself. This may lead to the notifier thread dying (if we were the last client),
            // and at least to us not firing any events anymore, in case somebody calls
            // NotifyAccessibleEvent, again
            comphelper::AccessibleEventNotifier::revokeClient( mnClientId );
            mnClientId = 0;
        }
    }
}

void SAL_CALL SvxRectCtlAccessibleContext::grabFocus()
{
    ::SolarMutexGuard aSolarGuard;
    ::osl::MutexGuard   aGuard( m_aMutex );

    ThrowExceptionIfNotAlive();

    mpRepr->GrabFocus();
}

sal_Int32 SvxRectCtlAccessibleContext::getForeground(  )
{
    ::SolarMutexGuard aSolarGuard;
    ::osl::MutexGuard   aGuard( m_aMutex );
    ThrowExceptionIfNotAlive();

    return sal_Int32(mpRepr->GetControlForeground());
}
sal_Int32 SvxRectCtlAccessibleContext::getBackground(  )
{
    ::SolarMutexGuard aSolarGuard;
    ::osl::MutexGuard   aGuard( m_aMutex );
    ThrowExceptionIfNotAlive();

    return sal_Int32(mpRepr->GetControlBackground());
}

// XServiceInfo
OUString SAL_CALL SvxRectCtlAccessibleContext::getImplementationName()
{
    return OUString( "com.sun.star.comp.ui.SvxRectCtlAccessibleContext" );
}

sal_Bool SAL_CALL SvxRectCtlAccessibleContext::supportsService( const OUString& sServiceName )
{
    return cppu::supportsService(this, sServiceName);
}

Sequence< OUString > SAL_CALL SvxRectCtlAccessibleContext::getSupportedServiceNames()
{
    const OUString sServiceName( "com.sun.star.accessibility.AccessibleContext" );
    return Sequence< OUString >( &sServiceName, 1 );
}

// XTypeProvider
Sequence< sal_Int8 > SAL_CALL SvxRectCtlAccessibleContext::getImplementationId()
{
    return css::uno::Sequence<sal_Int8>();
}

// XAccessibleSelection
void SAL_CALL SvxRectCtlAccessibleContext::selectAccessibleChild( sal_Int32 nIndex )
{
    ::SolarMutexGuard aSolarGuard;

    ::osl::MutexGuard   aGuard( m_aMutex );

    checkChildIndex( nIndex );

    ThrowExceptionIfNotAlive();

    const ChildIndexToPointData*    pData = IndexToPoint( nIndex );

    DBG_ASSERT( pData,
        "SvxRectCtlAccessibleContext::selectAccessibleChild(): this is an impossible state! Or at least should be..." );

    // this does all what is needed, including the change of the child's state!
    mpRepr->SetActualRP( pData->ePoint );
}

sal_Bool SAL_CALL SvxRectCtlAccessibleContext::isAccessibleChildSelected( sal_Int32 nIndex )
{
    ::osl::MutexGuard   aGuard( m_aMutex );

    checkChildIndex( nIndex );

    return nIndex == mnSelectedChild;
}

void SAL_CALL SvxRectCtlAccessibleContext::clearAccessibleSelection()
{
    SAL_WARN( "svx", "SvxRectCtlAccessibleContext::clearAccessibleSelection() is not possible!" );
}

void SAL_CALL SvxRectCtlAccessibleContext::selectAllAccessibleChildren()
{
    // guard in selectAccessibleChild()!

    selectAccessibleChild( 0 );     // default per definition
}

sal_Int32 SAL_CALL SvxRectCtlAccessibleContext::getSelectedAccessibleChildCount()
{
    ::osl::MutexGuard   aGuard( m_aMutex );

    return mnSelectedChild == NOCHILDSELECTED? 0 : 1;
}

Reference< XAccessible > SAL_CALL SvxRectCtlAccessibleContext::getSelectedAccessibleChild( sal_Int32 nIndex )
{
    ::osl::MutexGuard   aGuard( m_aMutex );

    if( nIndex || mnSelectedChild == NOCHILDSELECTED )
        // in our case only for the first (0) _selected_ child this is a valid request
        throw lang::IndexOutOfBoundsException();

    return getAccessibleChild( mnSelectedChild );
}

void SAL_CALL SvxRectCtlAccessibleContext::deselectAccessibleChild( sal_Int32 /*nIndex*/ )
{
    SAL_WARN( "svx", "SvxRectCtlAccessibleContext::deselectAccessibleChild() is not possible!" );

    throw lang::IndexOutOfBoundsException( "deselectAccessibleChild is not possible in this context", *this );   // never possible
}

// internals
void SvxRectCtlAccessibleContext::checkChildIndex( long nIndex )
{
    if( nIndex < 0 || nIndex >= getAccessibleChildCount() )
        throw lang::IndexOutOfBoundsException();
}

void SvxRectCtlAccessibleContext::FireChildFocus( RectPoint eButton )
{
    ::osl::MutexGuard   aGuard( m_aMutex );
    long nNew = PointToIndex( eButton );
    long nNumOfChildren = getAccessibleChildCount();
    if( nNew < nNumOfChildren )
    {
        // select new child
        mnSelectedChild = nNew;
        if( nNew != NOCHILDSELECTED )
        {
            if( mvChildren[ nNew ].is() )
                mvChildren[ nNew ]->FireFocusEvent();
        }
        else
        {
            const Reference< XInterface >   xSource( *this );
            Any                             aOld;
            Any                             aNew;
            aNew <<= AccessibleStateType::FOCUSED;
            if (mnClientId)
                comphelper::AccessibleEventNotifier::addEvent( mnClientId,
                                                               AccessibleEventObject( xSource, AccessibleEventId::STATE_CHANGED, aNew, aOld ) );
        }
    }
    else
        mnSelectedChild = NOCHILDSELECTED;
}
void SvxRectCtlAccessibleContext::selectChild( long nNew )
{
    ::osl::MutexGuard   aGuard( m_aMutex );
    if( nNew != mnSelectedChild )
    {
        long    nNumOfChildren = getAccessibleChildCount();
        if( nNew < nNumOfChildren )
        {   // valid index
            SvxRectCtlChildAccessibleContext*   pChild;
            if( mnSelectedChild != NOCHILDSELECTED )
            {   // deselect old selected child if one is selected
                pChild = mvChildren[ mnSelectedChild ].get();
                if( pChild )
                    pChild->setStateChecked( false );
            }

            // select new child
            mnSelectedChild = nNew;

            if( nNew != NOCHILDSELECTED )
            {
                if( mvChildren[ nNew ].is() )
                    mvChildren[ nNew ]->setStateChecked( true );
            }
        }
        else
            mnSelectedChild = NOCHILDSELECTED;
    }
}

void SvxRectCtlAccessibleContext::selectChild(RectPoint eButton )
{
    // no guard -> is done in next selectChild
    selectChild(PointToIndex( eButton ));
}

void SAL_CALL SvxRectCtlAccessibleContext::disposing()
{
    if( !rBHelper.bDisposed )
    {
        {
            ::osl::MutexGuard   aGuard( m_aMutex );
            mpRepr = nullptr;      // object dies with representation

            for (auto & rxChild : mvChildren)
                if( rxChild.is() )
                    rxChild->dispose();

            mvChildren.clear();
        }

        {
            ::osl::MutexGuard   aGuard( m_aMutex );

            // Send a disposing to all listeners.
            if ( mnClientId )
            {
                comphelper::AccessibleEventNotifier::revokeClientNotifyDisposing( mnClientId, *this );
                mnClientId =  0;
            }

            mxParent.clear();
        }
    }
}

tools::Rectangle SvxRectCtlAccessibleContext::GetBoundingBoxOnScreen()
{
    ::SolarMutexGuard aSolarGuard;
    ::osl::MutexGuard   aGuard( m_aMutex );

    ThrowExceptionIfNotAlive();

    return tools::Rectangle( mpRepr->GetParent()->OutputToScreenPixel( mpRepr->GetPosPixel() ), mpRepr->GetSizePixel() );
}

tools::Rectangle SvxRectCtlAccessibleContext::GetBoundingBox()
{
    ::SolarMutexGuard aSolarGuard;
    ::osl::MutexGuard   aGuard( m_aMutex );

    ThrowExceptionIfNotAlive();

    return tools::Rectangle( mpRepr->GetPosPixel(), mpRepr->GetSizePixel() );
}

void SvxRectCtlAccessibleContext::ThrowExceptionIfNotAlive()
{
    if( rBHelper.bDisposed || rBHelper.bInDispose )
        throw lang::DisposedException();
}

RectCtlAccessibleContext::RectCtlAccessibleContext(RectCtl* pRepr)
    : mpRepr(pRepr)
    , mnSelectedChild(NOCHILDSELECTED)
{
    {
        ::SolarMutexGuard aSolarGuard;
        msName = SvxResId( RID_SVXSTR_RECTCTL_ACC_CORN_NAME );
        msDescription = SvxResId( RID_SVXSTR_RECTCTL_ACC_CORN_DESCR );
    }

    mvChildren.resize(MAX_NUM_OF_CHILDREN);
}

RectCtlAccessibleContext::~RectCtlAccessibleContext()
{
    ensureDisposed();
}

IMPLEMENT_FORWARD_XINTERFACE2( RectCtlAccessibleContext, OAccessibleSelectionHelper, OAccessibleHelper_Base )
IMPLEMENT_FORWARD_XTYPEPROVIDER2( RectCtlAccessibleContext, OAccessibleSelectionHelper, OAccessibleHelper_Base )

Reference< XAccessible > SAL_CALL RectCtlAccessibleContext::getAccessibleAtPoint( const awt::Point& rPoint )
{
    ::osl::MutexGuard           aGuard( m_aMutex );

    Reference< XAccessible >    xRet;

    long nChild = mpRepr ? PointToIndex(mpRepr->GetApproxRPFromPixPt(rPoint)) : NOCHILDSELECTED;

    if (nChild != NOCHILDSELECTED)
        xRet = getAccessibleChild( nChild );

    return xRet;
}

// XAccessibleContext
sal_Int32 SAL_CALL RectCtlAccessibleContext::getAccessibleChildCount()
{
    ::osl::MutexGuard aGuard( m_aMutex );

    return SvxRectCtl::NO_CHILDREN;
}

Reference< XAccessible > SAL_CALL RectCtlAccessibleContext::getAccessibleChild( sal_Int32 nIndex )
{
    checkChildIndex( nIndex );

    Reference< XAccessible > xChild(mvChildren[ nIndex ].get());
    if( !xChild.is() )
    {
        ::SolarMutexGuard aSolarGuard;

        ::osl::MutexGuard   aGuard( m_aMutex );

        xChild = mvChildren[ nIndex ].get();

        if (!xChild.is() && mpRepr)
        {
            const ChildIndexToPointData*    p = IndexToPoint( nIndex );
            OUString aName(SvxResId(p->pResIdName));
            OUString aDescr(SvxResId(p->pResIdDescr));

            tools::Rectangle       aFocusRect( mpRepr->CalculateFocusRectangle( p->ePoint ) );

            RectCtlChildAccessibleContext*   pChild = new RectCtlChildAccessibleContext(this, aName,
                    aDescr, aFocusRect, nIndex );
            mvChildren[ nIndex ] = pChild;
            xChild = pChild;

            // set actual state
            if( mnSelectedChild == nIndex )
                pChild->setStateChecked( true );
        }
    }

    return xChild;
}

Reference< XAccessible > SAL_CALL RectCtlAccessibleContext::getAccessibleParent()
{
    if (mpRepr)
        return mpRepr->getAccessibleParent();
    return uno::Reference<css::accessibility::XAccessible>();
}

sal_Int16 SAL_CALL RectCtlAccessibleContext::getAccessibleRole()
{
    return AccessibleRole::PANEL;
}

OUString SAL_CALL RectCtlAccessibleContext::getAccessibleDescription()
{
    ::osl::MutexGuard   aGuard( m_aMutex );
    return msDescription + " Please use arrow key to selection.";
}

OUString SAL_CALL RectCtlAccessibleContext::getAccessibleName()
{
    ::osl::MutexGuard   aGuard( m_aMutex );
    return msName;
}

/** Return empty reference to indicate that the relation set is not
    supported.
*/
Reference< XAccessibleRelationSet > SAL_CALL RectCtlAccessibleContext::getAccessibleRelationSet()
{
    if (mpRepr)
        return mpRepr->get_accessible_relation_set();
    return uno::Reference<css::accessibility::XAccessibleRelationSet>();
}

Reference< XAccessibleStateSet > SAL_CALL RectCtlAccessibleContext::getAccessibleStateSet()
{
    ::osl::MutexGuard                       aGuard( m_aMutex );
    utl::AccessibleStateSetHelper*          pStateSetHelper = new utl::AccessibleStateSetHelper;

    if (mpRepr)
    {
        pStateSetHelper->AddState( AccessibleStateType::ENABLED );
        pStateSetHelper->AddState( AccessibleStateType::FOCUSABLE );
        if( mpRepr->HasFocus() )
            pStateSetHelper->AddState( AccessibleStateType::FOCUSED );
        pStateSetHelper->AddState( AccessibleStateType::OPAQUE );

        pStateSetHelper->AddState( AccessibleStateType::SHOWING );

        if( mpRepr->IsVisible() )
            pStateSetHelper->AddState( AccessibleStateType::VISIBLE );
    }
    else
        pStateSetHelper->AddState( AccessibleStateType::DEFUNC );

    return pStateSetHelper;
}

void SAL_CALL RectCtlAccessibleContext::grabFocus()
{
    ::SolarMutexGuard aSolarGuard;
    ::osl::MutexGuard   aGuard( m_aMutex );

    if (mpRepr)
        mpRepr->GrabFocus();
}

sal_Int32 RectCtlAccessibleContext::getForeground()
{
    ::SolarMutexGuard aSolarGuard;
    ::osl::MutexGuard   aGuard( m_aMutex );

    //see SvxRectCtl::Paint
    const StyleSettings& rStyles = Application::GetSettings().GetStyleSettings();
    return sal_Int32(rStyles.GetLabelTextColor());
}

sal_Int32 RectCtlAccessibleContext::getBackground(  )
{
    ::SolarMutexGuard aSolarGuard;
    ::osl::MutexGuard   aGuard( m_aMutex );

    //see SvxRectCtl::Paint
    const StyleSettings& rStyles = Application::GetSettings().GetStyleSettings();
    return sal_Int32(rStyles.GetDialogColor());
}

// XAccessibleSelection
void RectCtlAccessibleContext::implSelect(sal_Int32 nIndex, bool bSelect)
{
    ::SolarMutexGuard aSolarGuard;

    ::osl::MutexGuard   aGuard( m_aMutex );

    checkChildIndex( nIndex );

    const ChildIndexToPointData*    pData = IndexToPoint( nIndex );

    DBG_ASSERT(pData, "RectCtlAccessibleContext::selectAccessibleChild(): this is an impossible state! Or at least should be...");

    if (mpRepr)
    {
        if (bSelect)
        {
            // this does all what is needed, including the change of the child's state!
            mpRepr->SetActualRP( pData->ePoint );
        }
        else
        {
            SAL_WARN( "svx", "RectCtlAccessibleContext::clearAccessibleSelection() is not possible!" );
        }
    }
}

bool RectCtlAccessibleContext::implIsSelected( sal_Int32 nIndex )
{
    ::osl::MutexGuard   aGuard( m_aMutex );

    checkChildIndex( nIndex );

    return nIndex == mnSelectedChild;
}

// internals
void RectCtlAccessibleContext::checkChildIndex( long nIndex )
{
    if( nIndex < 0 || nIndex >= getAccessibleChildCount() )
        throw lang::IndexOutOfBoundsException();
}

void RectCtlAccessibleContext::FireChildFocus( RectPoint eButton )
{
    ::osl::MutexGuard   aGuard( m_aMutex );
    long nNew = PointToIndex( eButton );
    long nNumOfChildren = getAccessibleChildCount();
    if( nNew < nNumOfChildren )
    {
        // select new child
        mnSelectedChild = nNew;
        if( nNew != NOCHILDSELECTED )
        {
            if( mvChildren[ nNew ].is() )
                mvChildren[ nNew ]->FireFocusEvent();
        }
        else
        {
            Any                             aOld;
            Any                             aNew;
            aNew <<= AccessibleStateType::FOCUSED;
            NotifyAccessibleEvent(AccessibleEventId::STATE_CHANGED, aOld, aNew);
        }
    }
    else
        mnSelectedChild = NOCHILDSELECTED;
}

void RectCtlAccessibleContext::selectChild( long nNew )
{
    ::osl::MutexGuard   aGuard( m_aMutex );
    if( nNew != mnSelectedChild )
    {
        long    nNumOfChildren = getAccessibleChildCount();
        if( nNew < nNumOfChildren )
        {   // valid index
            RectCtlChildAccessibleContext*   pChild;
            if( mnSelectedChild != NOCHILDSELECTED )
            {   // deselect old selected child if one is selected
                pChild = mvChildren[ mnSelectedChild ].get();
                if( pChild )
                    pChild->setStateChecked( false );
            }

            // select new child
            mnSelectedChild = nNew;

            if( nNew != NOCHILDSELECTED )
            {
                if( mvChildren[ nNew ].is() )
                    mvChildren[ nNew ]->setStateChecked( true );
            }
        }
        else
            mnSelectedChild = NOCHILDSELECTED;
    }
}

void RectCtlAccessibleContext::selectChild(RectPoint eButton )
{
    // no guard -> is done in next selectChild
    selectChild(PointToIndex( eButton ));
}

void SAL_CALL RectCtlAccessibleContext::disposing()
{
    ::osl::MutexGuard aGuard(m_aMutex);
    OAccessibleSelectionHelper::disposing();
    for (auto & rxChild : mvChildren)
    {
        if( rxChild.is() )
            rxChild->dispose();
    }
    mvChildren.clear();
    mpRepr = nullptr;
}

awt::Rectangle RectCtlAccessibleContext::implGetBounds()
{
    ::SolarMutexGuard aSolarGuard;
    ::osl::MutexGuard   aGuard( m_aMutex );

    awt::Rectangle aRet;

    if (mpRepr)
    {
        const Point   aOutPos;//( mpRepr->GetPosPixel() );
        Size          aOutSize( mpRepr->GetSize());

        aRet.X = aOutPos.X();
        aRet.Y = aOutPos.Y();
        aRet.Width = aOutSize.Width();
        aRet.Height = aOutSize.Height();
    }

    return aRet;
}

SvxRectCtlChildAccessibleContext::SvxRectCtlChildAccessibleContext(
    const Reference<XAccessible>&   rxParent,
    const vcl::Window&                       rParentWindow,
    const OUString&              rName,
    const OUString&              rDescription,
    const tools::Rectangle&                    rBoundingBox,
    long                                nIndexInParent ) :

    SvxRectCtlChildAccessibleContext_Base( maMutex ),
    msDescription( rDescription ),
    msName( rName ),
    mxParent(rxParent),
    maBoundingBox( rBoundingBox ),
    mrParentWindow( rParentWindow ),
    mnClientId( 0 ),
    mnIndexInParent( nIndexInParent ),
    mbIsChecked( false )
{
}


SvxRectCtlChildAccessibleContext::~SvxRectCtlChildAccessibleContext()
{

    if( IsAlive() )
    {
        osl_atomic_increment( &m_refCount );
        dispose();      // set mpRepr = NULL & release all children
    }
}

// XAccessible
Reference< XAccessibleContext> SAL_CALL SvxRectCtlChildAccessibleContext::getAccessibleContext()
{
    return this;
}

// XAccessibleComponent
sal_Bool SAL_CALL SvxRectCtlChildAccessibleContext::containsPoint( const awt::Point& rPoint )
{
    // no guard -> done in getBounds()
    return tools::Rectangle( Point( 0, 0 ), GetBoundingBox().GetSize() ).IsInside( VCLPoint( rPoint ) );
}

Reference< XAccessible > SAL_CALL SvxRectCtlChildAccessibleContext::getAccessibleAtPoint( const awt::Point& /*rPoint*/ )
{
    return Reference< XAccessible >();
}

awt::Rectangle SAL_CALL SvxRectCtlChildAccessibleContext::getBounds()
{
    // no guard -> done in getBoundingBox()
    return AWTRectangle( GetBoundingBox() );
}

awt::Point SAL_CALL SvxRectCtlChildAccessibleContext::getLocation()
{
    // no guard -> done in getBoundingBox()
    return AWTPoint( GetBoundingBox().TopLeft() );
}

awt::Point SAL_CALL SvxRectCtlChildAccessibleContext::getLocationOnScreen()
{
    // no guard -> done in getBoundingBoxOnScreen()
    return AWTPoint( GetBoundingBoxOnScreen().TopLeft() );
}

awt::Size SAL_CALL SvxRectCtlChildAccessibleContext::getSize()
{
    // no guard -> done in getBoundingBox()
    return AWTSize( GetBoundingBox().GetSize() );
}

void SAL_CALL SvxRectCtlChildAccessibleContext::grabFocus()
{
}

sal_Int32 SvxRectCtlChildAccessibleContext::getForeground(  )
{
    ::SolarMutexGuard aSolarGuard;
    ::osl::MutexGuard   aGuard( maMutex );
    ThrowExceptionIfNotAlive();
    return sal_Int32(mrParentWindow.GetControlForeground());
}
sal_Int32 SvxRectCtlChildAccessibleContext::getBackground(  )
{
    ::SolarMutexGuard aSolarGuard;
    ::osl::MutexGuard   aGuard( maMutex );

    ThrowExceptionIfNotAlive();
    return sal_Int32(mrParentWindow.GetControlBackground());
}

// XAccessibleContext
sal_Int32 SAL_CALL SvxRectCtlChildAccessibleContext::getAccessibleChildCount()
{
    return 0;
}

Reference< XAccessible > SAL_CALL SvxRectCtlChildAccessibleContext::getAccessibleChild( sal_Int32 /*nIndex*/ )
{
    throw lang::IndexOutOfBoundsException();
}

Reference< XAccessible > SAL_CALL SvxRectCtlChildAccessibleContext::getAccessibleParent()
{
    return mxParent;
}

sal_Int32 SAL_CALL SvxRectCtlChildAccessibleContext::getAccessibleIndexInParent()
{
   return mnIndexInParent;
}

sal_Int16 SAL_CALL SvxRectCtlChildAccessibleContext::getAccessibleRole()
{
    return AccessibleRole::RADIO_BUTTON;
}

OUString SAL_CALL SvxRectCtlChildAccessibleContext::getAccessibleDescription()
{
    ::osl::MutexGuard   aGuard( maMutex );
    return msDescription;
}

OUString SAL_CALL SvxRectCtlChildAccessibleContext::getAccessibleName()
{
    ::osl::MutexGuard   aGuard( maMutex );
    return msName;
}

/** Return empty reference to indicate that the relation set is not
    supported.
*/
Reference<XAccessibleRelationSet> SAL_CALL SvxRectCtlChildAccessibleContext::getAccessibleRelationSet()
{
    utl::AccessibleRelationSetHelper* pRelationSetHelper = new utl::AccessibleRelationSetHelper;
    uno::Reference< css::accessibility::XAccessibleRelationSet > xSet = pRelationSetHelper;
    if( mxParent.is() )
      {
        uno::Sequence< uno::Reference< uno::XInterface > > aSequence { mxParent };
        pRelationSetHelper->AddRelation( css::accessibility::AccessibleRelation( css::accessibility::AccessibleRelationType::MEMBER_OF, aSequence ) );

    }

    return xSet;
}

Reference< XAccessibleStateSet > SAL_CALL SvxRectCtlChildAccessibleContext::getAccessibleStateSet()
{
    ::osl::MutexGuard                       aGuard( maMutex );
    utl::AccessibleStateSetHelper*          pStateSetHelper = new utl::AccessibleStateSetHelper;

    if( IsAlive() )
    {
        if( mbIsChecked )
        {
            pStateSetHelper->AddState( AccessibleStateType::CHECKED );
//          pStateSetHelper->AddState( AccessibleStateType::SELECTED );
        }

        pStateSetHelper->AddState( AccessibleStateType::ENABLED );
        pStateSetHelper->AddState( AccessibleStateType::SENSITIVE );
        pStateSetHelper->AddState( AccessibleStateType::OPAQUE );
        pStateSetHelper->AddState( AccessibleStateType::SELECTABLE );
        pStateSetHelper->AddState( AccessibleStateType::SHOWING );
        pStateSetHelper->AddState( AccessibleStateType::VISIBLE );
    }
    else
        pStateSetHelper->AddState( AccessibleStateType::DEFUNC );

    return pStateSetHelper;
}

lang::Locale SAL_CALL SvxRectCtlChildAccessibleContext::getLocale()
{
    ::osl::MutexGuard                       aGuard( maMutex );
    if( mxParent.is() )
    {
        Reference< XAccessibleContext >     xParentContext( mxParent->getAccessibleContext() );
        if( xParentContext.is() )
            return xParentContext->getLocale();
    }

    //  No locale and no parent.  Therefore throw exception to indicate this
    //  cluelessness.
    throw IllegalAccessibleComponentStateException();
}

void SAL_CALL SvxRectCtlChildAccessibleContext::addAccessibleEventListener( const Reference< XAccessibleEventListener >& xListener )
{
    if (xListener.is())
    {
        ::osl::MutexGuard   aGuard( maMutex );
        if (!mnClientId)
            mnClientId = comphelper::AccessibleEventNotifier::registerClient( );
        comphelper::AccessibleEventNotifier::addEventListener( mnClientId, xListener );
    }
}


void SAL_CALL SvxRectCtlChildAccessibleContext::removeAccessibleEventListener( const Reference< XAccessibleEventListener >& xListener )
{
    if (xListener.is())
    {
        ::osl::MutexGuard   aGuard( maMutex );

        sal_Int32 nListenerCount = comphelper::AccessibleEventNotifier::removeEventListener( mnClientId, xListener );
        if ( !nListenerCount )
        {
            // no listeners anymore
            // -> revoke ourself. This may lead to the notifier thread dying (if we were the last client),
            // and at least to us not firing any events anymore, in case somebody calls
            // NotifyAccessibleEvent, again
            comphelper::AccessibleEventNotifier::revokeClient( mnClientId );
            mnClientId = 0;
        }
    }
}

// XAccessibleValue
Any SAL_CALL SvxRectCtlChildAccessibleContext::getCurrentValue()
{
    ThrowExceptionIfNotAlive();

    Any aRet;
    aRet <<= ( mbIsChecked? 1.0 : 0.0 );
    return aRet;
}

sal_Bool SAL_CALL SvxRectCtlChildAccessibleContext::setCurrentValue( const Any& /*aNumber*/ )
{
    return false;
}

Any SAL_CALL SvxRectCtlChildAccessibleContext::getMaximumValue()
{
    Any aRet;
    aRet <<= 1.0;
    return aRet;
}

Any SAL_CALL SvxRectCtlChildAccessibleContext::getMinimumValue()
{
    Any aRet;
    aRet <<= 0.0;
    return aRet;
}


// XAccessibleAction


sal_Int32 SvxRectCtlChildAccessibleContext::getAccessibleActionCount( )
{
    return 1;
}


sal_Bool SvxRectCtlChildAccessibleContext::doAccessibleAction ( sal_Int32 nIndex )
{
    ::osl::MutexGuard   aGuard( maMutex );

    if ( nIndex < 0 || nIndex >= getAccessibleActionCount() )
        throw IndexOutOfBoundsException();

    Reference<XAccessibleSelection> xSelection( mxParent, UNO_QUERY);

    xSelection->selectAccessibleChild(mnIndexInParent);

    return true;
}


OUString SvxRectCtlChildAccessibleContext::getAccessibleActionDescription ( sal_Int32 nIndex )
{
    ::osl::MutexGuard   aGuard( maMutex );

    if ( nIndex < 0 || nIndex >= getAccessibleActionCount() )
        throw IndexOutOfBoundsException();

    return OUString("select");
}


Reference< XAccessibleKeyBinding > SvxRectCtlChildAccessibleContext::getAccessibleActionKeyBinding( sal_Int32 nIndex )
{
    ::osl::MutexGuard   aGuard( maMutex );

    if ( nIndex < 0 || nIndex >= getAccessibleActionCount() )
        throw IndexOutOfBoundsException();

    return Reference< XAccessibleKeyBinding >();
}

// XServiceInfo
OUString SAL_CALL SvxRectCtlChildAccessibleContext::getImplementationName()
{
    return OUString( "com.sun.star.comp.ui.SvxRectCtlChildAccessibleContext" );
}

sal_Bool SAL_CALL SvxRectCtlChildAccessibleContext::supportsService( const OUString& sServiceName )
{
    return cppu::supportsService(this, sServiceName);
}

Sequence< OUString > SAL_CALL SvxRectCtlChildAccessibleContext::getSupportedServiceNames()
{
    const OUString sServiceName ("com.sun.star.accessibility.AccessibleContext");
    return Sequence< OUString >( &sServiceName, 1 );
}

// XTypeProvider
Sequence< sal_Int8 > SAL_CALL SvxRectCtlChildAccessibleContext::getImplementationId()
{
    return css::uno::Sequence<sal_Int8>();
}

void SvxRectCtlChildAccessibleContext::CommitChange( const AccessibleEventObject& rEvent )
{
    if (mnClientId)
        comphelper::AccessibleEventNotifier::addEvent( mnClientId, rEvent );
}

void SAL_CALL SvxRectCtlChildAccessibleContext::disposing()
{
    if( !rBHelper.bDisposed )
    {
        ::osl::MutexGuard   aGuard( maMutex );

        // Send a disposing to all listeners.
        if ( mnClientId )
        {
            comphelper::AccessibleEventNotifier::revokeClientNotifyDisposing( mnClientId, *this );
            mnClientId =  0;
        }

        mxParent.clear();
    }
}

void SvxRectCtlChildAccessibleContext::ThrowExceptionIfNotAlive()
{
    if( rBHelper.bDisposed || rBHelper.bInDispose )
        throw lang::DisposedException();
}

tools::Rectangle SvxRectCtlChildAccessibleContext::GetBoundingBoxOnScreen()
{
    ::osl::MutexGuard   aGuard( maMutex );

    // no ThrowExceptionIfNotAlive() because its done in GetBoundingBox()
    tools::Rectangle           aRect( GetBoundingBox() );

    return tools::Rectangle( mrParentWindow.OutputToScreenPixel( aRect.TopLeft() ), aRect.GetSize() );
}

tools::Rectangle const & SvxRectCtlChildAccessibleContext::GetBoundingBox()
{
    // no guard necessary, because no one changes maBoundingBox after creating it
    ThrowExceptionIfNotAlive();

    return maBoundingBox;
}

void SvxRectCtlChildAccessibleContext::setStateChecked( bool bChecked )
{
    if( mbIsChecked != bChecked )
    {
        mbIsChecked = bChecked;

        const Reference< XInterface >   xSource( *this );

        Any                             aOld;
        Any                             aNew;
        Any&                            rMod = bChecked? aNew : aOld;

        //Send the STATE_CHANGED(Focused) event to accessible
        rMod <<= AccessibleStateType::FOCUSED;
        CommitChange( AccessibleEventObject( xSource, AccessibleEventId::STATE_CHANGED, aNew, aOld ) );

        rMod <<= AccessibleStateType::CHECKED;

        CommitChange( AccessibleEventObject( xSource, AccessibleEventId::STATE_CHANGED, aNew, aOld ) );
    }
}

void SvxRectCtlChildAccessibleContext::FireFocusEvent()
{
    const Reference< XInterface >   xSource( *this );
    Any                             aOld;
    Any                             aNew;
    aNew <<= AccessibleStateType::FOCUSED;
    CommitChange( AccessibleEventObject( xSource, AccessibleEventId::STATE_CHANGED, aNew, aOld ) );
}

RectCtlChildAccessibleContext::RectCtlChildAccessibleContext(
    const Reference<XAccessible>&   rxParent,
    const OUString&              rName,
    const OUString&              rDescription,
    const tools::Rectangle& rBoundingBox,
    long nIndexInParent )
    : msDescription( rDescription )
    , msName( rName )
    , mxParent(rxParent)
    , maBoundingBox( rBoundingBox )
    , mnIndexInParent( nIndexInParent )
    , mbIsChecked( false )
{
}

RectCtlChildAccessibleContext::~RectCtlChildAccessibleContext()
{
    ensureDisposed();
}

Reference< XAccessible > SAL_CALL RectCtlChildAccessibleContext::getAccessibleAtPoint( const awt::Point& /*rPoint*/ )
{
    return Reference< XAccessible >();
}

void SAL_CALL RectCtlChildAccessibleContext::grabFocus()
{
}

sal_Int32 RectCtlChildAccessibleContext::getForeground(  )
{
    ::SolarMutexGuard aSolarGuard;
    ::osl::MutexGuard   aGuard( m_aMutex );

    //see SvxRectCtl::Paint
    const StyleSettings& rStyles = Application::GetSettings().GetStyleSettings();
    return sal_Int32(rStyles.GetLabelTextColor());
}

sal_Int32 RectCtlChildAccessibleContext::getBackground(  )
{
    ::SolarMutexGuard aSolarGuard;
    ::osl::MutexGuard   aGuard( m_aMutex );

    //see SvxRectCtl::Paint
    const StyleSettings& rStyles = Application::GetSettings().GetStyleSettings();
    return sal_Int32(rStyles.GetDialogColor());
}

// XAccessibleContext
sal_Int32 SAL_CALL RectCtlChildAccessibleContext::getAccessibleChildCount()
{
    return 0;
}

Reference< XAccessible > SAL_CALL RectCtlChildAccessibleContext::getAccessibleChild( sal_Int32 /*nIndex*/ )
{
    throw lang::IndexOutOfBoundsException();
}

Reference< XAccessible > SAL_CALL RectCtlChildAccessibleContext::getAccessibleParent()
{
    return mxParent;
}

sal_Int16 SAL_CALL RectCtlChildAccessibleContext::getAccessibleRole()
{
    return AccessibleRole::RADIO_BUTTON;
}

OUString SAL_CALL RectCtlChildAccessibleContext::getAccessibleDescription()
{
    ::osl::MutexGuard   aGuard( m_aMutex );
    return msDescription;
}

OUString SAL_CALL RectCtlChildAccessibleContext::getAccessibleName()
{
    ::osl::MutexGuard   aGuard( m_aMutex );
    return msName;
}

/** Return empty reference to indicate that the relation set is not
    supported.
*/
Reference<XAccessibleRelationSet> SAL_CALL RectCtlChildAccessibleContext::getAccessibleRelationSet()
{
    utl::AccessibleRelationSetHelper* pRelationSetHelper = new utl::AccessibleRelationSetHelper;
    uno::Reference< css::accessibility::XAccessibleRelationSet > xSet = pRelationSetHelper;
    if( mxParent.is() )
      {
        uno::Sequence< uno::Reference< uno::XInterface > > aSequence { mxParent };
        pRelationSetHelper->AddRelation( css::accessibility::AccessibleRelation( css::accessibility::AccessibleRelationType::MEMBER_OF, aSequence ) );

    }

    return xSet;
}

Reference< XAccessibleStateSet > SAL_CALL RectCtlChildAccessibleContext::getAccessibleStateSet()
{
    ::osl::MutexGuard                       aGuard( m_aMutex );
    utl::AccessibleStateSetHelper*          pStateSetHelper = new utl::AccessibleStateSetHelper;

    if (!rBHelper.bDisposed)
    {
        if( mbIsChecked )
        {
            pStateSetHelper->AddState( AccessibleStateType::CHECKED );
        }

        pStateSetHelper->AddState( AccessibleStateType::ENABLED );
        pStateSetHelper->AddState( AccessibleStateType::SENSITIVE );
        pStateSetHelper->AddState( AccessibleStateType::OPAQUE );
        pStateSetHelper->AddState( AccessibleStateType::SELECTABLE );
        pStateSetHelper->AddState( AccessibleStateType::SHOWING );
        pStateSetHelper->AddState( AccessibleStateType::VISIBLE );
    }
    else
        pStateSetHelper->AddState( AccessibleStateType::DEFUNC );

    return pStateSetHelper;
}

// XAccessibleValue
Any SAL_CALL RectCtlChildAccessibleContext::getCurrentValue()
{
    Any aRet;
    aRet <<= ( mbIsChecked? 1.0 : 0.0 );
    return aRet;
}

sal_Bool SAL_CALL RectCtlChildAccessibleContext::setCurrentValue( const Any& /*aNumber*/ )
{
    return false;
}

Any SAL_CALL RectCtlChildAccessibleContext::getMaximumValue()
{
    Any aRet;
    aRet <<= 1.0;
    return aRet;
}

Any SAL_CALL RectCtlChildAccessibleContext::getMinimumValue()
{
    Any aRet;
    aRet <<= 0.0;
    return aRet;
}


// XAccessibleAction


sal_Int32 RectCtlChildAccessibleContext::getAccessibleActionCount( )
{
    return 1;
}


sal_Bool RectCtlChildAccessibleContext::doAccessibleAction ( sal_Int32 nIndex )
{
    ::osl::MutexGuard   aGuard( m_aMutex );

    if ( nIndex < 0 || nIndex >= getAccessibleActionCount() )
        throw IndexOutOfBoundsException();

    Reference<XAccessibleSelection> xSelection( mxParent, UNO_QUERY);

    xSelection->selectAccessibleChild(mnIndexInParent);

    return true;
}


OUString RectCtlChildAccessibleContext::getAccessibleActionDescription ( sal_Int32 nIndex )
{
    ::osl::MutexGuard   aGuard( m_aMutex );

    if ( nIndex < 0 || nIndex >= getAccessibleActionCount() )
        throw IndexOutOfBoundsException();

    return OUString("select");
}


Reference< XAccessibleKeyBinding > RectCtlChildAccessibleContext::getAccessibleActionKeyBinding( sal_Int32 nIndex )
{
    ::osl::MutexGuard   aGuard( m_aMutex );

    if ( nIndex < 0 || nIndex >= getAccessibleActionCount() )
        throw IndexOutOfBoundsException();

    return Reference< XAccessibleKeyBinding >();
}

void SAL_CALL RectCtlChildAccessibleContext::disposing()
{
    OAccessibleComponentHelper::disposing();
    mxParent.clear();
}

awt::Rectangle RectCtlChildAccessibleContext::implGetBounds(  )
{
    // no guard necessary, because no one changes maBoundingBox after creating it
    return AWTRectangle(maBoundingBox);
}

void RectCtlChildAccessibleContext::setStateChecked( bool bChecked )
{
    if( mbIsChecked != bChecked )
    {
        mbIsChecked = bChecked;

        Any                             aOld;
        Any                             aNew;
        Any&                            rMod = bChecked? aNew : aOld;

        //Send the STATE_CHANGED(Focused) event to accessible
        rMod <<= AccessibleStateType::FOCUSED;
        NotifyAccessibleEvent(AccessibleEventId::STATE_CHANGED, aOld, aNew);

        rMod <<= AccessibleStateType::CHECKED;

        NotifyAccessibleEvent(AccessibleEventId::STATE_CHANGED, aOld, aNew);
    }
}

void RectCtlChildAccessibleContext::FireFocusEvent()
{
    Any                             aOld;
    Any                             aNew;
    aNew <<= AccessibleStateType::FOCUSED;
    NotifyAccessibleEvent(AccessibleEventId::STATE_CHANGED, aOld, aNew);
}

IMPLEMENT_FORWARD_XINTERFACE2( RectCtlChildAccessibleContext, OAccessibleComponentHelper, OAccessibleHelper_Base_3 )
IMPLEMENT_FORWARD_XTYPEPROVIDER2( RectCtlChildAccessibleContext, OAccessibleComponentHelper, OAccessibleHelper_Base_3 )

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
