// BM_INSERT/DELETE/BUILD tested on 1000 words(|word| <= 100)
// BM_INSERT      - summary time of Insert
// BM_DELETE      - summary time of Delete
// BM_BUILD       - time of only Build after insertions

// BM_FIND        - time of Find with fixed dict in `war and peace`, text was appended to himself many times (~1.1GB)
// BM_RANDOM_FIND - time of Find in random text (len = 1GB) with random dict (100 words, |word| <= 100 characters)

//Hyperscan
//  BM_INSERT: 0.001926
//  BM_DELETE: 0.000439
//  BM_BUILD: 0.126492
//  BM_RANDOM_FIND: 0.13844
//  BM_FIND: 0.446024


//Hyperscan
//  BM_INSERT: 0.002192
//  BM_DELETE: 0.000651
//  BM_BUILD: 0.118698
//  cnt: 8
//  BM_RANDOM_FIND: 0.014088
//  BM_PACKETS_1_5k: 75365; time in sec: 0.029461
//  BM_FIND: 0.000784
//
//BoostScan
//  BM_INSERT: 0.048486
//  BM_DELETE: 0.011661
//  BM_BUILD: 0
//  cnt: 8
//  BM_RANDOM_FIND: 2.59314
//  BM_PACKETS_1_5k: 75365; time in sec: 6.36321
//  BM_FIND: 0.183115

#ifndef BENCHMARKS_H
#define BENCHMARKS_H

void startBM();

#endif // BENCHMARKS_H
