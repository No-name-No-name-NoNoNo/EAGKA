#!/bin/bash

# Create progress file
TIMESTAMP=$(date +%Y%m%d%H%M%S)

# Large-scale experiment parameters
declare -A AREA_CONFIG    # Area size configuration
declare -A NODE_RANGE     # Node count range

AREA_CONFIG[0,0]="200"    
AREA_CONFIG[0,1]="200"
AREA_CONFIG[0,2]="80"

AREA_CONFIG[1,0]="350"   
AREA_CONFIG[1,1]="350"
AREA_CONFIG[1,2]="150"

AREA_CONFIG[2,0]="450"    
AREA_CONFIG[2,1]="450"
AREA_CONFIG[2,2]="200"

AREA_CONFIG[3,0]="600"   
AREA_CONFIG[3,1]="600"
AREA_CONFIG[3,2]="250"

NODE_RANGE[0,0]="5"       
NODE_RANGE[0,1]="10"

NODE_RANGE[1,0]="15"      
NODE_RANGE[1,1]="25"

NODE_RANGE[2,0]="30"      
NODE_RANGE[2,1]="40"

NODE_RANGE[3,0]="45"      
NODE_RANGE[3,1]="55"

LINK_QUALITIES=("LOS")


# Number of experiments for each configuration
NUM_EXPERIMENTS=200

# Configure logging and compilation options
export NS_LOG='wifi-adhoc-UAV-experiment=info|prefix_time|prefix_func:wifi-adhoc-app=info|prefix_time|prefix_func';
export CXXFLAGS="-g -std=c++03 -fpermissive"
export LDFLAGS="-lsqlite3"

echo "Large-scale experiment started..."

# Generate command file
CMD_FILE="commands_${TIMESTAMP}.txt"
echo "Generating all experiment commands..."

for area_idx in {0..3}
do
    areaLength=${AREA_CONFIG[$area_idx,0]}
    areaWidth=${AREA_CONFIG[$area_idx,1]}
    areaHeight=${AREA_CONFIG[$area_idx,2]}
    
    node_min=${NODE_RANGE[$area_idx,0]}
    node_max=${NODE_RANGE[$area_idx,1]}
    
    echo "========================================="
    echo "Area configuration: ${areaLength}*${areaWidth}*${areaHeight}"
    echo "Node range: ${node_min}-${node_max}"
    echo "========================================="
    
    for linkQuality in "${LINK_QUALITIES[@]}"
    do
        echo "Link quality: $linkQuality"
        
        for nodeCount in $(seq $node_min $node_max)
        do
            echo "Node count: $nodeCount"
            
            for ((run=1; run<=NUM_EXPERIMENTS; run++))
            do
                seed=$((RANDOM % 1000000))
                # Write in multiple lines for readability
                echo "../../waf --run 'REGKA-Ours \
                --numNodes=$nodeCount \
                --areaLength=$areaLength \
                --areaWidth=$areaWidth \
                --areaHeight=$areaHeight \
                --linkQuality=$linkQuality \
                --run=$run \
                --RngRun=$seed'" >> $CMD_FILE
            done
        done
    done
done


parallel --delay 0.2 -j 120 < $CMD_FILE > run.log 2>&1

# Merge results
python Analyze.py

echo -e "\nAll experiments completed!"