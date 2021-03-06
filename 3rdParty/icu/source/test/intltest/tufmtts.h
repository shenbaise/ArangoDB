/********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 2008-2013, International Business Machines Corporation
 * and others. All Rights Reserved.
 ********************************************************************/

#ifndef __INTLTESTTIMEUNITTEST__
#define __INTLTESTTIMEUNITTEST__ 


#if !UCONFIG_NO_FORMATTING

#include "unicode/utypes.h"
#include "unicode/locid.h"
#include "intltest.h"

/**
 * Test basic functionality of various API functions
 **/
class TimeUnitTest: public IntlTest {
    void runIndexedTest( int32_t index, UBool exec, const char* &name, char* par = NULL );  

public:
    /**
     * Performs basic tests
     **/
    void testBasic();

    /**
     * Performs API tests
     **/
    void testAPI();

    /**
     * Performs tests for Greek
     * This tests that requests for short unit names correctly fall back 
     * to long unit names for a locale where the locale data does not 
     * provide short unit names. As of CLDR 1.9, Greek is one such language.
     **/
    void testGreekWithFallback();

    /**
     * Performs tests for Greek
     * This tests that if the plural count listed in time unit format does not
     * match those in the plural rules for the locale, those plural count in
     * time unit format will be ingored and subsequently, fall back will kick in
     * which is tested above.
     * Without data sanitization, setNumberFormat() would crash.
     * As of CLDR shiped in ICU4.8, Greek is one such language.
     */
    void testGreekWithSanitization();

};

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif
