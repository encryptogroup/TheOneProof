set -e

cd benchmark_results

MODE_E1=false
MODE_E2=false
MODE_ALL=false
RELATEDWORKS=false
for arg; do
    if [ "$arg" = "E1" ]; then
        MODE_E1=true
    elif [ "$arg" = "E2" ]; then
        MODE_E2=true
    elif [ "$arg" = "all" ]; then
        MODE_ALL=true
    elif [ "$arg" = "relatedworks" ]; then
        RELATEDWORKS=true
    fi
done

if $MODE_E1; then
    echo "Providing outputs for experiment E1..."
    echo ""
elif $MODE_E2; then
    echo "Providing outputs for experiment E2..."
    echo ""
elif $MODE_ALL; then
    echo "Providing outputs for all tables, figures, etc."
    echo ""
else
    echo "Please provide 'E1', 'E2', or 'all' as flag, e.g., './parse_reproduced.sh E1'"
    exit 1
fi

if $RELATEDWORKS; then
    echo "Using reproduced data for related works"
else
    echo "Using fallback data for related works, included in this repository"
fi
echo ""

if $MODE_ALL; then
    echo "Plotting Fig. 9 to benchmark_results/plots/plot_theoretical.pdf and printing numbers used in last para of §5.1..."
    echo "-----------------------------------------------------------------------------------------------------------------"
    python3 plot_theoretical.py
    echo "-----------------------------------------------------------------------------------------------------------------"
    echo ""
fi

if $MODE_ALL; then
    echo "Printing maximum memory utilization (for our protocol) as stated in footnote 7..."
    echo "-----------------------------------------------------------------------------------------------------------------"
    python3 analyze_memory.py r silent
    echo "-----------------------------------------------------------------------------------------------------------------"
    echo ""
fi

if $MODE_E1 || $MODE_ALL; then
    echo "Printing Table 3..."
    echo "-----------------------------------------------------------------------------------------------------------------"
    python3 table_general.py r
    echo "-----------------------------------------------------------------------------------------------------------------"
    echo ""
fi

if $MODE_E1 || $MODE_ALL; then
    echo "Printing Table 4..."
    echo "-----------------------------------------------------------------------------------------------------------------"
    python3 table_size.py r
    echo "-----------------------------------------------------------------------------------------------------------------"
    echo ""
fi

if $MODE_E1 || $MODE_ALL; then
    echo "Plotting Fig. 10 to benchmark_results/plots/plot_parties_square.pdf and printing numbers used in end of §5.2.6..."
    echo "-----------------------------------------------------------------------------------------------------------------"
    python3 plot_parties.py r
    echo "-----------------------------------------------------------------------------------------------------------------"
    echo ""
fi

if $MODE_E2 || $MODE_ALL; then
    echo "Printing Table 5..."
    echo "-----------------------------------------------------------------------------------------------------------------"
    if $RELATEDWORKS; then
        python3 table_asterisk.py r r-asterisk
    else
        echo "!!fallback data used for Asterisk, this may significantly skew the run time improvement factors!!"
        python3 table_asterisk.py r
    fi
    echo "-----------------------------------------------------------------------------------------------------------------"
    echo ""
fi

if $MODE_ALL; then
    echo "Printing Table 6..."
    echo "-----------------------------------------------------------------------------------------------------------------"
    python3 table_dotp.py r
    echo "-----------------------------------------------------------------------------------------------------------------"
    echo ""
fi

if $MODE_ALL; then
    echo "Printing Table 7..."
    echo "-----------------------------------------------------------------------------------------------------------------"
    if $RELATEDWORKS; then
        python3 table_RSS_and_SPDZ2k.py r r-RSS r-SPDZ2k
    else
        echo "!!fallback data used for RSS/SPDZ2k, this may significantly skew the run time increases/decreases!!"
        python3 table_RSS_and_SPDZ2k.py r
    fi
    echo "-----------------------------------------------------------------------------------------------------------------"
    echo ""
fi

cd ..
