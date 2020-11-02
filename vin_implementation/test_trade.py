## Perform imports
import os
import threading
import warnings
import sys
from vin_implementation.utilities import *
from pandas.core.common import SettingWithCopyWarning

# Add path for cloud ex import
additional_paths = ['/root/CloudExchange', '/root/']
for p in additional_paths:
    sys.path.append(p)

import cloud_ex

# Start Redis and its Python API.
warnings.simplefilter(action="ignore", category=SettingWithCopyWarning)
os.system("redis-server --daemonize yes")
time.sleep(1)

# Start Trader
T = getTrader()
print(config['gateway_ip'])

GLOBALS = {
    'NUM_SHARES': 1,
    'BIN_INTERVAL_MS': 250,  # interval to bin the data with
    'WAIT_INTERVAL_SECONDS': 0.25,
    'BACKTEST_LOOKBACK_PERIOD_SECONDS': 60 * 2,  # amount of historical data to backtest on
    'MAX_NUM_ORDERS': 60 * 2 * 4,  # how long the algo will trade for, in # of bins
}

top_symbols, symbol_ranks = get_top_n_symbols_by_volume(default_symbols, 60, 10)
getSymbols()


##############################################
# Note a fully specified strategy is called an "algo" and consists of a tuple of (strategy, kwargs) where kwargs provide
# the parameters for the strategy
def mean_reversion_algo(ma, t):
    # Helper function for making the "algo" data structure
    # NOTE this returns a TUPLE of (strategy , kwargs)
    return (strategies_shay.strategies['mean_reversion'].strategy, {'ma': ma, 'threshold': t})


def momentum_algo(p1, p2, t):
    # NOTE this returns a TUPLE of (strategy , kwargs)
    return (strategies_shay.strategies['momentum'].strategy, {'p1': p1, 'p2': p2, 'threshold': t})


def random_buy_algo(p):
    return (strategies_shay.strategies['random_buy'].strategy, {'p': p})


def random_sell_algo(p):
    return (strategies_shay.strategies['random_sell'].strategy, {'p': p})


# Define an "Algo Bank", which holds the (strategy, kwargs) pair, indexed by a unique identifier
algo_bank = {
    'mr_10_3': mean_reversion_algo(10, 3),
    'mr_10_5': mean_reversion_algo(10, 5),
    'mr_10_10': mean_reversion_algo(10, 10),
    'mo_5_5_1': momentum_algo(0.5, 0.5, 1),
    'mo_5_5_3': momentum_algo(0.5, 0.5, 3),
    'mo_8_2_1': momentum_algo(0.8, 0.2, 1),
    'mo_8_2_3': momentum_algo(0.8, 0.2, 3),
    'rb_10': random_buy_algo(0.1),
    'rb_50': random_buy_algo(0.5),
    'rs_10': random_sell_algo(0.1),
    'rs_50': random_sell_algo(0.5),

}


def backtest_strategy_with_symbol(data, strategy, symbol, params):
    """
    Backtests a given strategy, kwarg pair on a diven symbol using the last seconds_in_past seconds of historical data
    """

    # bin_interval_ms = 500
    # summarize_historical_trades_df(symbol_historical_trades_df, bin_interval_ms)

    # Initial capital and shares we will use for the backtest.
    init_capital = 100000
    init_shares = 1000

    # Set up trading algorithm parameters.
    num_shares = 10

    # Set up the strategy
    trader = None
    algo = strategy(trader, [symbol],
                    GLOBALS['BIN_INTERVAL_MS'])  # instantiates the strategy which is subclass of algorithmic_trader

    # Run the backtest.
    roi, action_list = algo.backtest(data,
                                     num_shares,
                                     init_capital,
                                     init_shares,
                                     **params)

    print("Algo ROI={}%".format(roi))
    return roi, action_list


def backtest_algobank_on_symbols(algobank, symbols):
    """
    Backtests all algos ( strategy, kwarg pairs) on each symbol in symbols, over the last seconds_in_past seconds
    of data

    Returns the results in a sorted list with algos ranked by roi
    """
    results = []

    for symbol in symbols:

        print("\n\nBacktesting symbol: {}".format(symbol))

        end_time_ms = int(time.time() * 1e3)
        start_time_ms = end_time_ms - int(GLOBALS['BACKTEST_LOOKBACK_PERIOD_SECONDS'] * 1e3)
        symbol_trades_vec = cloud_ex.VectorTrade()
        cloud_ex.MarketDataAPI.PullTrades(config['project_id'], config['bigtable_id'],
                                          config['table_name'], symbol, start_time_ms,
                                          end_time_ms, symbol_trades_vec)
        print("There are a total of {} trades for {} symbol".format(len(symbol_trades_vec), symbol))
        symbol_historical_trades_df = TradeDF(symbol_trades_vec)
        symbol_historical_trades_df = symbol_historical_trades_df.sort_values(by="CreationTimestamp")

        for (algoname, algo) in algobank.items():
            strategy, params = algo
            print("algo={}".format(algoname))
            roi, action_list = backtest_strategy_with_symbol(symbol_historical_trades_df, strategy, symbol, params)
            results.append([roi, symbol, algoname, action_list])
            print("Made {} trades with roi: {}".format(len(action_list), roi))

    results.sort(key=lambda x: x[0], reverse=True)
    return results


##########################################

def run_and_evaluate_algorithm(**kwargs):
    """
    Intended as target of new thread()
    1. Launches the algo on the symbol and starts trading
    2. should calculate ROI of the algo
    4. Writes ROI and submitted order ids to disk
    """
    name = kwargs['name']
    strategy = kwargs['strategy']
    strategy_parameters = kwargs['strategy_parameters']
    num_shares = kwargs['num_shares']
    max_num_orders = kwargs['max_num_orders']
    symbol = kwargs['symbol']
    trader = kwargs['trader']  # reference to the 1 trader instance connnected to cloudX

    # create the AlgorithmicTrader Object
    algo = strategy(trader, [symbol], bin_interval_ms=GLOBALS['BIN_INTERVAL_MS'])

    # get and set id for this trader  (for logging purposes)
    trader_id = name + "_" + str(time.time()).split(".")[0]

    # Calculate portfolio state pre-trading
    pass

    # start and finish trading
    order_ids = algo.trade(symbol, num_shares, max_num_orders, GLOBALS['WAIT_INTERVAL_SECONDS'], trader_id=trader_id,
                           **strategy_parameters)
    # can SIMULATE if we want

    # calculate portfolio state post-trading
    pass

    # time.sleep(10)
    # write the order ids to a log file
    print(trader_id + "_order_ids", json.dumps(order_ids))
    # write the ROI information a log file
    pass


def run_algorithm_in_thread(kwargs):
    # create the thread
    t = threading.Thread(target=run_and_evaluate_algorithm,
                         kwargs=kwargs)
    # start the thread
    t.start()
    # return it
    return t


def deploy_top_N_algorithms(trader, ranked_algos, N, num_shares, max_num_orders):
    sublist = ranked_algos[0:N]
    ts = []
    for to_deploy in sublist:
        roi, symbol, algoname, _ = to_deploy
        strategy, strategy_parameters = algo_bank[algoname]

        arguments = {
            'name': algoname,
            'strategy': strategy,
            'strategy_parameters': strategy_parameters,
            'num_shares': num_shares,
            'max_num_orders': max_num_orders,
            'symbol': symbol,
            'trader': trader,
        }
        ts.append(run_algorithm_in_thread(arguments))

    return ts


def trade_algorithmically(trader, algobank, symbols, N=5):
    while True:
        print("tradeloop", "\n{}, Doing backtest".format(time.time()))
        ranked = backtest_algobank_on_symbols(algobank, symbols)

        fname = "backtest_results_{}".format(time.time())
        print(fname, json.dumps(ranked))
        print("tradeloop", "backtest_results=>")
        print("tradeloop", "\n{}\n".format(json.dumps(ranked[0:N])))

        print("tradeloop", "\n{}, Launching Trading".format(time.time()))
        algo_threads = deploy_top_N_algorithms(trader, ranked, N, GLOBALS['NUM_SHARES'], GLOBALS['MAX_NUM_ORDERS'])
        for t in algo_threads:
            t.join()
