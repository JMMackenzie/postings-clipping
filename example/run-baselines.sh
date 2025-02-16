PISA=../pisa-baseline/build/

INDEX=base-index/

effectiveness() {
## BASELINES
for qtype in dev dev2; do
for k in 1000; do
    for index in bp-unicoil-tilde; do
        for algo in wand maxscore ls_maxscore; do
            #printf $index" "$k" "$algo" ";
            $PISA/bin/evaluate_queries \
                -e block_simdbp \
                -i $INDEX/$index.block_simdbp.idx \
                -w $INDEX/$index.fixed-40.bmw \
                -s quantized \
                --weighted \
                -k $k \
                -a $algo \
                --documents $INDEX/$index.docmap \
                -q ../data/queries/marco-v2/no-split/$index"."$qtype".query.ints" > runs/$index-$algo-$qtype.trec
        done
    done
done
done
}

latency(){
for k in 10 1000; do
    for index in bp-unicoil-tilde; do
        for algo in wand maxscore ls_maxscore; do
            printf $index" "$k" "$algo" ";
            $PISA/bin/queries \
                -e block_simdbp \
                -i $INDEX/$index.block_simdbp.idx \
                -w $INDEX/$index.fixed-40.bmw \
                -s quantized \
                --weighted \
                --threads 16 \
                -k $k \
                -a $algo \
                -q ../data/queries/marco-v2/no-split/$index".joined.query.ints"
        done
    done
done

for k in 10 1000; do
    for index in bp-unicoil-tilde; do
        for algo in block_max_wand; do
            printf $index" "$k" "$algo" ";
            $PISA/bin/queries \
                -e block_simdbp \
                -i $INDEX/$index.block_simdbp.idx \
                -w $INDEX/$index.fixed-40.bmw \
                -s quantized \
                --weighted \
                --threads 16 \
                -k $k \
                -a $algo \
                -q ../data/queries/marco-v2/no-split/$index".joined.query.ints"
        done
    done
done

}

effectiveness
latency
