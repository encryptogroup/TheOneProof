set -e

cd benchmark_results

if [[ $1 == "relatedworks" ]]; then
    echo "Using reproduced data for related works"
else
    echo "Using fallback data for related works, included in this repository"
fi
echo ""

echo "Plotting Fig. 9 to benchmark_results/plots/plot_theoretical.pdf and printing numbers used in last para of §5.1..."
echo "-----------------------------------------------------------------------------------------------------------------"
python3 plot_theoretical.py
echo "-----------------------------------------------------------------------------------------------------------------"
echo ""

echo "Printing maximum memory utilization (for our protocol) as stated in footnote 7..."
echo "-----------------------------------------------------------------------------------------------------------------"
python3 analyze_memory.py r silent
echo "-----------------------------------------------------------------------------------------------------------------"
echo ""

echo "Printing Table 3..."
echo "-----------------------------------------------------------------------------------------------------------------"
python3 table_general.py r
echo "-----------------------------------------------------------------------------------------------------------------"
echo ""

echo "Printing Table 4..."
echo "-----------------------------------------------------------------------------------------------------------------"
python3 table_size.py r
echo "-----------------------------------------------------------------------------------------------------------------"
echo ""

echo "Plotting Fig. 10 to benchmark_results/plots/plot_parties_square.pdf and printing numbers used in end of §5.2.6..."
echo "-----------------------------------------------------------------------------------------------------------------"
python3 plot_parties.py r
echo "-----------------------------------------------------------------------------------------------------------------"
echo ""

echo "Printing Table 5..."
echo "-----------------------------------------------------------------------------------------------------------------"
if [[ $1 == "relatedworks" ]]; then
    python3 table_asterisk.py r r-asterisk
else
    echo "!!fallback data used for Asterisk, this may significantly skew the run time improvement factors!!"
    python3 table_asterisk.py r
fi
echo "-----------------------------------------------------------------------------------------------------------------"
echo ""

echo "Printing Table 6..."
echo "-----------------------------------------------------------------------------------------------------------------"
python3 table_dotp.py r
echo "-----------------------------------------------------------------------------------------------------------------"
echo ""

echo "Printing Table 7..."
echo "-----------------------------------------------------------------------------------------------------------------"
if [[ $1 == "relatedworks" ]]; then
    python3 table_RSS_and_SPDZ2k.py r r-RSS r-SPDZ2k
else
    echo "!!fallback data used for RSS/SPDZ2k, this may significantly skew the run time increases/decreases!!"
    python3 table_RSS_and_SPDZ2k.py r
fi
echo "-----------------------------------------------------------------------------------------------------------------"
echo ""
