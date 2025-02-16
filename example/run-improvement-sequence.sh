PISA=../pisa-decomposition/build

latency() {
for x in split-index clipped-index; do

    INDEX=$x

    for k in 10 1000; do
        for index in bp-unicoil-tilde; do
            for algo in wand maxscore ls_maxscore wand_prime maxscore_prime ls_maxscore_prime pair_aware_wand pair_aware_maxscore pair_aware_wand_prime pair_aware_maxscore_prime; do
                printf $index" "$k" "$algo" "$x;
                $PISA/bin/queries \
                    -e block_simdbp \
                    -i $INDEX/$index.block_simdbp.idx \
                    -w $INDEX/$index.fixed-40.bmw \
                    -s quantized \
                    --terms $INDEX/$index.termlex \
                    --threads 16 \
                    -k $k \
                    -a $algo \
                    -q ../data/queries/marco-v2/split/$index".joined.query" 
            done
        done
    done

    for k in 10 1000; do
        for index in bp-unicoil-tilde; do
            for algo in block_max_wand block_max_wand_prime pair_aware_block_max_wand pair_aware_block_max_wand_prime; do
                printf $index" "$k" "$algo" "$x;
                $PISA/bin/queries \
                    -e block_simdbp \
                    -i $INDEX/$index.block_simdbp.idx \
                    -w $INDEX/$index.fixed-40.bmw \
                    -s quantized \
                    --terms $INDEX/$index.termlex \
                    --threads 16 \
                    -k $k \
                    -a $algo \
                    -q ../data/queries/marco-v2/split/$index".joined.query" 
            done
        done
    done
done
}


effectiveness(){
for x in split-index clipped-index; do

    INDEX=$x

    for k in 1000; do
        for index in bp-unicoil-tilde; do
            for algo in wand maxscore ls_maxscore wand_prime maxscore_prime ls_maxscore_prime pair_aware_wand pair_aware_maxscore pair_aware_wand_prime pair_aware_maxscore_prime; do
                printf $index" "$k" "$algo" ";
                $PISA/bin/evaluate_queries \
                    -e block_simdbp \
                    -i $INDEX/$index.block_simdbp.idx \
                    -w $INDEX/$index.fixed-40.bmw \
                    -s quantized \
                    --terms $INDEX/$index.termlex \
                    -k $k \
                    -a $algo \
                    --documents $INDEX/$index.docmap \
                    -q ../data/queries/marco-v2/split/$index".joined.query" > runs/$algo-$x
            done
        done
    done

    for k in 1000; do
        for index in bp-unicoil-tilde; do
            for algo in block_max_wand block_max_wand_prime pair_aware_block_max_wand pair_aware_block_max_wand_prime; do
                printf $index" "$k" "$algo" ";
                $PISA/bin/evaluate_queries \
                    -e block_simdbp \
                    -i $INDEX/$index.block_simdbp.idx \
                    -w $INDEX/$index.fixed-40.bmw \
                    -s quantized \
                    --terms $INDEX/$index.termlex \
                    -k $k \
                    -a $algo \
                    --documents $INDEX/$index.docmap \
                    -q ../data/queries/marco-v2/split/$index".joined.query" > runs/$algo-$x
            done
        done
    done

done
}

effectiveness
latency
