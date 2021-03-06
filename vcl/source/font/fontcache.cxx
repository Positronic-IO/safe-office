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

#include <svdata.hxx>

#include <fontinstance.hxx>
#include <impfontcache.hxx>
#include <PhysicalFontCollection.hxx>
#include <PhysicalFontFace.hxx>
#include <PhysicalFontFamily.hxx>

size_t ImplFontCache::IFSD_Hash::operator()( const FontSelectPattern& rFSD ) const
{
    return rFSD.hashCode();
}

bool ImplFontCache::IFSD_Equal::operator()(const FontSelectPattern& rA, const FontSelectPattern& rB) const
{
    // check normalized font family name
    if( rA.maSearchName != rB.maSearchName )
        return false;

    // check font transformation
    if( (rA.mnHeight       != rB.mnHeight)
    ||  (rA.mnWidth        != rB.mnWidth)
    ||  (rA.mnOrientation  != rB.mnOrientation) )
        return false;

    // check mapping relevant attributes
    if( (rA.mbVertical     != rB.mbVertical)
    ||  (rA.meLanguage     != rB.meLanguage) )
        return false;

    // check font face attributes
    if( (rA.GetWeight()       != rB.GetWeight())
    ||  (rA.GetItalic()    != rB.GetItalic())
//  ||  (rA.meFamily       != rB.meFamily) // TODO: remove this mostly obsolete member
    ||  (rA.GetPitch()     != rB.GetPitch()) )
        return false;

    // check style name
    if( rA.GetStyleName() != rB.GetStyleName() )
        return false;

    // Symbol fonts may recode from one type to another So they are only
    // safely equivalent for equal targets
    if (rA.IsSymbolFont() && rB.IsSymbolFont())
    {
        if (rA.maTargetName != rB.maTargetName)
            return false;
    }

    // check for features
    if ((rA.maTargetName.indexOf(FontSelectPatternAttributes::FEAT_PREFIX)
         != -1 ||
         rB.maTargetName.indexOf(FontSelectPatternAttributes::FEAT_PREFIX)
         != -1) && rA.maTargetName != rB.maTargetName)
        return false;

    if (rA.mbEmbolden != rB.mbEmbolden)
        return false;

    if (rA.maItalicMatrix != rB.maItalicMatrix)
        return false;

    return true;
}

ImplFontCache::ImplFontCache()
:   mpLastHitCacheEntry( nullptr ),
    mnRef0Count( 0 )
{}

ImplFontCache::~ImplFontCache()
{
    for (auto const& fontInstance : maFontInstanceList)
    {
        LogicalFontInstance* pFontInstance = fontInstance.second;
        if (pFontInstance->mnRefCount)
            pFontInstance->mpFontCache = nullptr;
        else
            delete pFontInstance;
    }
}

LogicalFontInstance* ImplFontCache::GetFontInstance( PhysicalFontCollection const * pFontList,
    const vcl::Font& rFont, const Size& rSize, float fExactHeight )
{
    // initialize internal font request object
    FontSelectPattern aFontSelData(rFont, rFont.GetFamilyName(), rSize, fExactHeight);
    return GetFontInstance( pFontList, aFontSelData );
}

LogicalFontInstance* ImplFontCache::GetFontInstance( PhysicalFontCollection const * pFontList,
    FontSelectPattern& aFontSelData )
{
    LogicalFontInstance *pFontInstance = nullptr;
    PhysicalFontFamily* pFontFamily = nullptr;

    // check if a directly matching logical font instance is already cached,
    // the most recently used font usually has a hit rate of >50%
    if (mpLastHitCacheEntry && IFSD_Equal()(aFontSelData, mpLastHitCacheEntry->GetFontSelectPattern()))
        pFontInstance = mpLastHitCacheEntry;
    else
    {
        FontInstanceList::iterator it = maFontInstanceList.find( aFontSelData );
        if( it != maFontInstanceList.end() )
            pFontInstance = (*it).second;
    }

    if( !pFontInstance ) // no direct cache hit
    {
        // find the best matching logical font family and update font selector accordingly
        pFontFamily = pFontList->FindFontFamily( aFontSelData );
        SAL_WARN_IF( (pFontFamily == nullptr), "vcl", "ImplFontCache::Get() No logical font found!" );
        if( pFontFamily )
        {
            aFontSelData.maSearchName = pFontFamily->GetSearchName();

            // check if an indirectly matching logical font instance is already cached
            FontInstanceList::iterator it = maFontInstanceList.find( aFontSelData );
            if( it != maFontInstanceList.end() )
                pFontInstance = (*it).second;
        }
    }

    if( pFontInstance ) // cache hit => use existing font instance
    {
        // increase the font instance's reference count
        pFontInstance->Acquire();
    }
    else if (pFontFamily) // still no cache hit => create a new font instance
    {
        PhysicalFontFace* pFontData = pFontFamily->FindBestFontFace(aFontSelData);

        // create a new logical font instance from this physical font face
        pFontInstance = pFontData->CreateFontInstance( aFontSelData );
        pFontInstance->mpFontCache = this;

        // if we're substituting from or to a symbol font we may need a symbol
        // conversion table
        if( pFontData->IsSymbolFont() || aFontSelData.IsSymbolFont() )
        {
            if( aFontSelData.maTargetName != aFontSelData.maSearchName )
                pFontInstance->mpConversion = ConvertChar::GetRecodeData( aFontSelData.maTargetName, aFontSelData.maSearchName );
        }

#ifdef MACOSX
        //It might be better to dig out the font version of the target font
        //to see if it's a modern re-coded apple symbol font in case that
        //font shows up on a different platform
        if (!pFontInstance->mpConversion &&
            aFontSelData.maTargetName.equalsIgnoreAsciiCase("symbol") &&
            aFontSelData.maSearchName.equalsIgnoreAsciiCase("symbol"))
        {
            pFontInstance->mpConversion = ConvertChar::GetRecodeData( "Symbol", "AppleSymbol" );
        }
#endif

        // add the new entry to the cache
#ifndef NDEBUG
        auto aResult =
#endif
        maFontInstanceList.insert({aFontSelData, pFontInstance});
        assert(aResult.second);
    }

    mpLastHitCacheEntry = pFontInstance;
    return pFontInstance;
}

LogicalFontInstance* ImplFontCache::GetGlyphFallbackFont( PhysicalFontCollection const * pFontCollection,
    FontSelectPattern& rFontSelData, int nFallbackLevel, OUString& rMissingCodes )
{
    // get a candidate font for glyph fallback
    // unless the previously selected font got a device specific substitution
    // e.g. PsPrint Arial->Helvetica for udiaeresis when Helvetica doesn't support it
    if( nFallbackLevel >= 1)
    {
        PhysicalFontFamily* pFallbackData = nullptr;

        //fdo#33898 If someone has EUDC installed then they really want that to
        //be used as the first-choice glyph fallback seeing as it's filled with
        //private area codes with don't make any sense in any other font so
        //prioritize it here if it's available. Ideally we would remove from
        //rMissingCodes all the glyphs which it is able to resolve as an
        //optimization, but that's tricky to achieve cross-platform without
        //sufficient heavy-weight code that's likely to undo the value of the
        //optimization
        if (nFallbackLevel == 1)
            pFallbackData = pFontCollection->FindFontFamily("EUDC");
        if (!pFallbackData)
            pFallbackData = pFontCollection->GetGlyphFallbackFont(rFontSelData, rMissingCodes, nFallbackLevel-1);
        // escape when there are no font candidates
        if( !pFallbackData  )
            return nullptr;
        // override the font name
        rFontSelData.SetFamilyName( pFallbackData->GetFamilyName() );
        // clear the cached normalized name
        rFontSelData.maSearchName.clear();
    }

    LogicalFontInstance* pFallbackFont = GetFontInstance( pFontCollection, rFontSelData );
    return pFallbackFont;
}

void ImplFontCache::Acquire(LogicalFontInstance* pFontInstance)
{
    assert(pFontInstance->mpFontCache == this);
    assert(IsFontInList(pFontInstance) && "ImplFontCache::Acquire() - font absent in the cache");

    if (0 == pFontInstance->mnRefCount++)
        --mnRef0Count;
}

void ImplFontCache::Release(LogicalFontInstance* pFontInstance)
{
    static const int FONTCACHE_MAX = getenv("LO_TESTNAME") ? 1 : 50;

    assert(pFontInstance->mpFontCache == this);
    assert(IsFontInList(pFontInstance) && "ImplFontCache::Release() - font absent in the cache");
    assert(pFontInstance->mnRefCount > 0 && "ImplFontCache::Release() - font refcount underflow");
    if( --pFontInstance->mnRefCount > 0 )
        return;

    if (++mnRef0Count < FONTCACHE_MAX)
        return;

    assert(CountUnreferencedEntries() == mnRef0Count);

    // remove unused entries from font instance cache
    FontInstanceList::iterator it_next = maFontInstanceList.begin();
    while( it_next != maFontInstanceList.end() )
    {
        LogicalFontInstance* pFontEntry = (*it_next).second;
        if( pFontEntry->mnRefCount > 0 )
        {
            ++it_next;
            continue;
        }

        it_next = maFontInstanceList.erase(it_next);
        delete pFontEntry;
        --mnRef0Count;
        assert(mnRef0Count>=0 && "ImplFontCache::Release() - refcount0 underflow");

        if (mpLastHitCacheEntry == pFontEntry)
            mpLastHitCacheEntry = nullptr;
    }

    assert(mnRef0Count==0 && "ImplFontCache::Release() - refcount0 mismatch");
}

bool ImplFontCache::IsFontInList(const LogicalFontInstance* pFont) const
{
    auto Pred = [pFont](const FontInstanceList::value_type& el) -> bool { return el.second == pFont; };
    return std::find_if(maFontInstanceList.begin(), maFontInstanceList.end(), Pred) != maFontInstanceList.end();
}

int ImplFontCache::CountUnreferencedEntries() const
{
    size_t nCount = 0;
    // count unreferenced entries
    for (auto const& fontInstance : maFontInstanceList)
    {
        const LogicalFontInstance* pFontEntry = fontInstance.second;
        if (pFontEntry->mnRefCount > 0)
            continue;
        ++nCount;
    }
    return nCount;
}

void ImplFontCache::Invalidate()
{
    assert(CountUnreferencedEntries() == mnRef0Count);

    // delete unreferenced entries
    for (auto const& fontInstance : maFontInstanceList)
    {
        LogicalFontInstance* pFontEntry = fontInstance.second;
        if( pFontEntry->mnRefCount > 0 )
        {
            // These fonts will become orphans after clearing the list below;
            // allow them to control their life from now on and wish good luck :)
            pFontEntry->mpFontCache = nullptr;
            continue;
        }

        delete pFontEntry;
        --mnRef0Count;
    }

    // #112304# make sure the font cache is really clean
    mpLastHitCacheEntry = nullptr;
    maFontInstanceList.clear();

    assert(mnRef0Count==0 && "ImplFontCache::Invalidate() - mnRef0Count non-zero");
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
