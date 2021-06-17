/* $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================*/

/*****************************************************************************

File name: sls_normal_distr_array.cpp

Author: Sergey Sheetlin

Contents: Normal distribution array for P-values calculation

******************************************************************************/

#include <ncbi_pch.hpp>
#include <corelib/ncbistl.hpp>
#include "sls_normal_distr_array.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

BEGIN_SCOPE(Sls)

#define NORMAL_DISTR_ARRAY_DIM 1000
static double normal_distr_array_for_P_values_calculation[NORMAL_DISTR_ARRAY_DIM+1]={
7.65613E-24,        9.36789E-24,        1.14578E-23,        1.40085E-23,        1.71202E-23,        2.09147E-23,        2.55403E-23,        3.11764E-23,        3.80412E-23,        4.63993E-23,        
5.65713E-23,        6.89460E-23,        8.39943E-23,        1.02287E-22,        1.24513E-22,        1.51510E-22,        1.84286E-22,        2.24065E-22,        2.72323E-22,        3.30842E-22,        
4.01778E-22,        4.87731E-22,        5.91836E-22,        7.17879E-22,        8.70421E-22,        1.05496E-21,        1.27811E-21,        1.54786E-21,        1.87380E-21,        2.26747E-21,        
2.74276E-21,        3.31637E-21,        4.00835E-21,        4.84280E-21,        5.84866E-21,        7.06064E-21,        8.52041E-21,        1.02779E-20,        1.23930E-20,        1.49375E-20,        
1.79973E-20,        2.16752E-20,        2.60945E-20,        3.14024E-20,        3.77751E-20,        4.54230E-20,        5.45977E-20,        6.55996E-20,        7.87873E-20,        9.45889E-20,        
1.13515E-19,        1.36173E-19,        1.63290E-19,        1.95730E-19,        2.34522E-19,        2.80890E-19,        3.36294E-19,        4.02466E-19,        4.81469E-19,        5.75753E-19,        
6.88227E-19,        8.22349E-19,        9.82220E-19,        1.17271E-18,        1.39959E-18,        1.66970E-18,        1.99115E-18,        2.37356E-18,        2.82828E-18,        3.36880E-18,        
4.01103E-18,        4.77381E-18,        5.67940E-18,        6.75412E-18,        8.02904E-18,        9.54085E-18,        1.13328E-17,        1.34561E-17,        1.59709E-17,        1.89481E-17,        
2.24715E-17,        2.66396E-17,        3.15683E-17,        3.73942E-17,        4.42777E-17,        5.24077E-17,        6.20060E-17,        7.33332E-17,        8.66955E-17,        1.02452E-16,        
1.21025E-16,        1.42908E-16,        1.68681E-16,        1.99025E-16,        2.34733E-16,        2.76740E-16,        3.26136E-16,        3.84196E-16,        4.52415E-16,        5.32536E-16,        
6.26600E-16,        7.36988E-16,        8.66482E-16,        1.01833E-15,        1.19631E-15,        1.40485E-15,        1.64909E-15,        1.93503E-15,        2.26966E-15,        2.66111E-15,        
3.11884E-15,        3.65387E-15,        4.27899E-15,        5.00909E-15,        5.86146E-15,        6.85617E-15,        8.01653E-15,        9.36958E-15,        1.09467E-14,        1.27842E-14,        
1.49244E-14,        1.74159E-14,        2.03154E-14,        2.36883E-14,        2.76104E-14,        3.21691E-14,        3.74658E-14,        4.36175E-14,        5.07592E-14,        5.90471E-14,        
6.86613E-14,        7.98094E-14,        9.27312E-14,        1.07703E-13,        1.25042E-13,        1.45116E-13,        1.68347E-13,        1.95220E-13,        2.26293E-13,        2.62210E-13,        
3.03707E-13,        3.51634E-13,        4.06965E-13,        4.70817E-13,        5.44473E-13,        6.29405E-13,        7.27300E-13,        8.40091E-13,        9.69994E-13,        1.11954E-12,        
1.29164E-12,        1.48962E-12,        1.71726E-12,        1.97891E-12,        2.27953E-12,        2.62480E-12,        3.02117E-12,        3.47604E-12,        3.99782E-12,        4.59613E-12,        
5.28191E-12,        6.06763E-12,        6.96751E-12,        7.99771E-12,        9.17663E-12,        1.05252E-11,        1.20673E-11,        1.38298E-11,        1.58436E-11,        1.81435E-11,        
2.07691E-11,        2.37654E-11,        2.71834E-11,        3.10807E-11,        3.55229E-11,        4.05841E-11,        4.63482E-11,        5.29104E-11,        6.03780E-11,        6.88727E-11,        
7.85317E-11,        8.95104E-11,        1.01984E-10,        1.16150E-10,        1.32233E-10,        1.50484E-10,        1.71186E-10,        1.94661E-10,        2.21269E-10,        2.51415E-10,        
2.85557E-10,        3.24208E-10,        3.67948E-10,        4.17426E-10,        4.73372E-10,        5.36607E-10,        6.08052E-10,        6.88740E-10,        7.79832E-10,        8.82627E-10,        
9.98583E-10,        1.12933E-09,        1.27670E-09,        1.44274E-09,        1.62974E-09,        1.84026E-09,        2.07716E-09,        2.34365E-09,        2.64330E-09,        2.98010E-09,        
3.35850E-09,        3.78348E-09,        4.26058E-09,        4.79597E-09,        5.39654E-09,        6.06996E-09,        6.82476E-09,        7.67043E-09,        8.61754E-09,        9.67784E-09,        
1.08644E-08,        1.21916E-08,        1.36757E-08,        1.53345E-08,        1.71879E-08,        1.92577E-08,        2.15685E-08,        2.41471E-08,        2.70235E-08,        3.02309E-08,        
3.38058E-08,        3.77888E-08,        4.22247E-08,        4.71631E-08,        5.26586E-08,        5.87717E-08,        6.55690E-08,        7.31242E-08,        8.02883E-08,        8.94616E-08,        
9.96443E-08,        1.10943E-07,        1.23475E-07,        1.37369E-07,        1.52768E-07,        1.69827E-07,        1.88717E-07,        2.09628E-07,        2.32766E-07,        2.58357E-07,        
2.86651E-07,        3.17921E-07,        3.52466E-07,        3.90613E-07,        4.32721E-07,        4.79183E-07,        5.30429E-07,        5.86929E-07,        6.49196E-07,        7.17791E-07,        
7.93328E-07,        8.76476E-07,        9.67965E-07,        1.06859E-06,        1.17922E-06,        1.30081E-06,        1.43437E-06,        1.58105E-06,        1.74205E-06,        1.91870E-06,        
2.11245E-06,        2.32488E-06,        2.55768E-06,        2.81271E-06,        3.09198E-06,        3.39767E-06,        3.73215E-06,        4.09798E-06,        4.49794E-06,        4.93505E-06,        
5.41254E-06,        5.93397E-06,        6.50312E-06,        7.12414E-06,        7.80146E-06,        8.53991E-06,        9.34467E-06,        1.02213E-05,        1.11760E-05,        1.22151E-05,        
1.33457E-05,        1.45755E-05,        1.59124E-05,        1.73653E-05,        1.89436E-05,        2.06575E-05,        2.25179E-05,        2.45364E-05,        2.67256E-05,        2.90991E-05,        
3.16712E-05,        3.44576E-05,        3.74749E-05,        4.07408E-05,        4.42745E-05,        4.80963E-05,        5.22282E-05,        5.66935E-05,        6.15172E-05,        6.67258E-05,        
7.23480E-05,        7.84142E-05,        8.49567E-05,        9.20101E-05,        9.96114E-05,        1.07800E-04,        1.16617E-04,        1.26108E-04,        1.36319E-04,        1.47302E-04,        
1.59109E-04,        1.71797E-04,        1.85427E-04,        2.00064E-04,        2.15773E-04,        2.32629E-04,        2.50707E-04,        2.70088E-04,        2.90857E-04,        3.13106E-04,        
3.36929E-04,        3.62429E-04,        3.89712E-04,        4.18892E-04,        4.50087E-04,        4.83424E-04,        5.19035E-04,        5.57061E-04,        5.97648E-04,        6.40953E-04,        
6.87138E-04,        7.36375E-04,        7.88846E-04,        8.44739E-04,        9.04255E-04,        9.67603E-04,        1.03500E-03,        1.10668E-03,        1.18289E-03,        1.26387E-03,        
1.34990E-03,        1.44124E-03,        1.53820E-03,        1.64106E-03,        1.75016E-03,        1.86581E-03,        1.98838E-03,        2.11821E-03,        2.25568E-03,        2.40118E-03,        
2.55513E-03,        2.71794E-03,        2.89007E-03,        3.07196E-03,        3.26410E-03,        3.46697E-03,        3.68111E-03,        3.90703E-03,        4.14530E-03,        4.39649E-03,        
4.66119E-03,        4.94002E-03,        5.23361E-03,        5.54262E-03,        5.86774E-03,        6.20967E-03,        6.56912E-03,        6.94685E-03,        7.34363E-03,        7.76025E-03,        
8.19754E-03,        8.65632E-03,        9.13747E-03,        9.64187E-03,        1.01704E-02,        1.07241E-02,        1.13038E-02,        1.19106E-02,        1.25455E-02,        1.32094E-02,        
1.39034E-02,        1.46287E-02,        1.53863E-02,        1.61774E-02,        1.70030E-02,        1.78644E-02,        1.87628E-02,        1.96993E-02,        2.06752E-02,        2.16917E-02,        
2.27501E-02,        2.38518E-02,        2.49979E-02,        2.61898E-02,        2.74289E-02,        2.87166E-02,        3.00540E-02,        3.14428E-02,        3.28841E-02,        3.43795E-02,        
3.59303E-02,        3.75380E-02,        3.92039E-02,        4.09295E-02,        4.27162E-02,        4.45655E-02,        4.64787E-02,        4.84572E-02,        5.05026E-02,        5.26161E-02,        
5.47993E-02,        5.70534E-02,        5.93799E-02,        6.17802E-02,        6.42555E-02,        6.68072E-02,        6.94366E-02,        7.21450E-02,        7.49337E-02,        7.78038E-02,        
8.07567E-02,        8.37933E-02,        8.69150E-02,        9.01227E-02,        9.34175E-02,        9.68005E-02,        1.00273E-01,        1.03835E-01,        1.07488E-01,        1.11232E-01,        
1.15070E-01,        1.19000E-01,        1.23024E-01,        1.27143E-01,        1.31357E-01,        1.35666E-01,        1.40071E-01,        1.44572E-01,        1.49170E-01,        1.53864E-01,        
1.58655E-01,        1.63543E-01,        1.68528E-01,        1.73609E-01,        1.78786E-01,        1.84060E-01,        1.89430E-01,        1.94895E-01,        2.00454E-01,        2.06108E-01,        
2.11855E-01,        2.17695E-01,        2.23627E-01,        2.29650E-01,        2.35762E-01,        2.41964E-01,        2.48252E-01,        2.54627E-01,        2.61086E-01,        2.67629E-01,        
2.74253E-01,        2.80957E-01,        2.87740E-01,        2.94599E-01,        3.01532E-01,        3.08538E-01,        3.15614E-01,        3.22758E-01,        3.29969E-01,        3.37243E-01,        
3.44578E-01,        3.51973E-01,        3.59424E-01,        3.66928E-01,        3.74484E-01,        3.82089E-01,        3.89739E-01,        3.97432E-01,        4.05165E-01,        4.12936E-01,        
4.20740E-01,        4.28576E-01,        4.36441E-01,        4.44330E-01,        4.52242E-01,        4.60172E-01,        4.68119E-01,        4.76078E-01,        4.84047E-01,        4.92022E-01,        
5.00000E-01,        5.07978E-01,        5.15953E-01,        5.23922E-01,        5.31881E-01,        5.39828E-01,        5.47758E-01,        5.55670E-01,        5.63559E-01,        5.71424E-01,        
5.79260E-01,        5.87064E-01,        5.94835E-01,        6.02568E-01,        6.10261E-01,        6.17911E-01,        6.25516E-01,        6.33072E-01,        6.40576E-01,        6.48027E-01,        
6.55422E-01,        6.62757E-01,        6.70031E-01,        6.77242E-01,        6.84386E-01,        6.91462E-01,        6.98468E-01,        7.05401E-01,        7.12260E-01,        7.19043E-01,        
7.25747E-01,        7.32371E-01,        7.38914E-01,        7.45373E-01,        7.51748E-01,        7.58036E-01,        7.64238E-01,        7.70350E-01,        7.76373E-01,        7.82305E-01,        
7.88145E-01,        7.93892E-01,        7.99546E-01,        8.05105E-01,        8.10570E-01,        8.15940E-01,        8.21214E-01,        8.26391E-01,        8.31472E-01,        8.36457E-01,        
8.41345E-01,        8.46136E-01,        8.50830E-01,        8.55428E-01,        8.59929E-01,        8.64334E-01,        8.68643E-01,        8.72857E-01,        8.76976E-01,        8.81000E-01,        
8.84930E-01,        8.88768E-01,        8.92512E-01,        8.96165E-01,        8.99727E-01,        9.03200E-01,        9.06582E-01,        9.09877E-01,        9.13085E-01,        9.16207E-01,        
9.19243E-01,        9.22196E-01,        9.25066E-01,        9.27855E-01,        9.30563E-01,        9.33193E-01,        9.35745E-01,        9.38220E-01,        9.40620E-01,        9.42947E-01,        
9.45201E-01,        9.47384E-01,        9.49497E-01,        9.51543E-01,        9.53521E-01,        9.55435E-01,        9.57284E-01,        9.59070E-01,        9.60796E-01,        9.62462E-01,        
9.64070E-01,        9.65620E-01,        9.67116E-01,        9.68557E-01,        9.69946E-01,        9.71283E-01,        9.72571E-01,        9.73810E-01,        9.75002E-01,        9.76148E-01,        
9.77250E-01,        9.78308E-01,        9.79325E-01,        9.80301E-01,        9.81237E-01,        9.82136E-01,        9.82997E-01,        9.83823E-01,        9.84614E-01,        9.85371E-01,        
9.86097E-01,        9.86791E-01,        9.87455E-01,        9.88089E-01,        9.88696E-01,        9.89276E-01,        9.89830E-01,        9.90358E-01,        9.90863E-01,        9.91344E-01,        
9.91802E-01,        9.92240E-01,        9.92656E-01,        9.93053E-01,        9.93431E-01,        9.93790E-01,        9.94132E-01,        9.94457E-01,        9.94766E-01,        9.95060E-01,        
9.95339E-01,        9.95604E-01,        9.95855E-01,        9.96093E-01,        9.96319E-01,        9.96533E-01,        9.96736E-01,        9.96928E-01,        9.97110E-01,        9.97282E-01,        
9.97445E-01,        9.97599E-01,        9.97744E-01,        9.97882E-01,        9.98012E-01,        9.98134E-01,        9.98250E-01,        9.98359E-01,        9.98462E-01,        9.98559E-01,        
9.98650E-01,        9.98736E-01,        9.98817E-01,        9.98893E-01,        9.98965E-01,        9.99032E-01,        9.99096E-01,        9.99155E-01,        9.99211E-01,        9.99264E-01,        
9.99313E-01,        9.99359E-01,        9.99402E-01,        9.99443E-01,        9.99481E-01,        9.99517E-01,        9.99550E-01,        9.99581E-01,        9.99610E-01,        9.99638E-01,        
9.99663E-01,        9.99687E-01,        9.99709E-01,        9.99730E-01,        9.99749E-01,        9.99767E-01,        9.99784E-01,        9.99800E-01,        9.99815E-01,        9.99828E-01,        
9.99841E-01,        9.99853E-01,        9.99864E-01,        9.99874E-01,        9.99883E-01,        9.99892E-01,        9.99900E-01,        9.99908E-01,        9.99915E-01,        9.99922E-01,        
9.99928E-01,        9.99933E-01,        9.99938E-01,        9.99943E-01,        9.99948E-01,        9.99952E-01,        9.99956E-01,        9.99959E-01,        9.99963E-01,        9.99966E-01,        
9.99968E-01,        9.99971E-01,        9.99973E-01,        9.99975E-01,        9.99977E-01,        9.99979E-01,        9.99981E-01,        9.99983E-01,        9.99984E-01,        9.99985E-01,        
9.99987E-01,        9.99988E-01,        9.99989E-01,        9.99990E-01,        9.99991E-01,        9.99991E-01,        9.99992E-01,        9.99993E-01,        9.99993E-01,        9.99994E-01,        
9.99995E-01,        9.99995E-01,        9.99996E-01,        9.99996E-01,        9.99996E-01,        9.99997E-01,        9.99997E-01,        9.99997E-01,        9.99997E-01,        9.99998E-01,        
9.99998E-01,        9.99998E-01,        9.99998E-01,        9.99998E-01,        9.99999E-01,        9.99999E-01,        9.99999E-01,        9.99999E-01,        9.99999E-01,        9.99999E-01,        
9.99999E-01,        9.99999E-01,        9.99999E-01,        9.99999E-01,        9.99999E-01,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        
1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        
1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        
1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        
1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        
1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        
1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        
1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        
1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        
1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        
1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        
1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        
1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        
1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        
1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        
1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        
1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        
1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        
1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        
1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        
1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        
1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        
1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        
1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        
1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        
1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        1.00000E+00,        
1.00000E+00
};

double* GetNormalDistrArrayForPvaluesCalculation(void)
{
    return normal_distr_array_for_P_values_calculation;
}

END_SCOPE(Sls)

END_SCOPE(blast)
END_NCBI_SCOPE

