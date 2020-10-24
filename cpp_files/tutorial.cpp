#include "trader/trader_api.h"
/* Setup and identity flags */
DEFINE_string(configuration_path, "/root/vm_config.json",
              "Read your configuration file from this path");

int main(int argc, char** argv) {
  // Getting VM and CLoudEX config.
  std::ifstream config_fstream;
  config_fstream.open(FLAGS_configuration_path.c_str());
  if (!config_fstream.is_open()) {
    VLOG(1) << "Failed to Open: " << FLAGS_configuration_path << std::endl;
    return -1;
  }
  Json::Value root;
  config_fstream >> root;
  config_fstream.close();
  std::string gateway_ip = root["gateway_ip"].asString();
  std::string client_id = root["client_id"].asString();
  std::string client_token = root["client_token"].asString();
  std::string project_id = root["project_id"].asString();
  std::string bigtable_id = root["bigtable_id"].asString();
  std::string table_name = root["table_name"].asString();

  // Start redis and flushall before trade object construction.
  system("redis-server --daemonize yes");
  system("redis-cli flushall");

  // **************************************************************************
  // * 1. Submitting Orders
  // **************************************************************************

  // * 1.1 Setup CloudEx Trader
  // This object will instantiate a connection to a gateway in CloudEx. This
  // Gateway will relay order to the matching engine and market data to your
  // client.
  // Arguments to `Trader` constructor:
  // - gateway_ip `str` - The IP address for the gateway assigned to you
  // - client_id `str` - Your client identifier
  // - client_token `str` - Your client token
  Trader trader(gateway_ip, client_id, client_token);

  // * 1.2 Submitting a Buy Order
  // We will create a buy order object and collect information on the order we
  // would like to place.
  // Arguments to Trader.SubmitOrder():
  //     symbol str - The symbol we would like to trade
  //     order cloud_ex.Order - The Order object that will be updated by
  //     submission info order_type cloud_ex.OrderType - The type of order to
  //     place, whether a limit (cloud_ex.OrderType.limit) or a market order
  //     (cloud_ex.OrderType.market) order_action cloud_ex.OrderType - Whether
  //     the order is a buy (cloud_ex.OrderAction.buy) or sell order
  //     (cloud_ex.OrderAction.sell) num_shares int - Number of shares to
  //     buy/sell price int - The order's price (integer)

  Order order;
  OrderType order_type;
  OrderAction order_action;
  OrderResult result;
  bool success;
  std::string symbol;
  std::int32_t num_shares;
  std::int32_t price;

  order_type = OrderType::limit;
  order_action = OrderAction::buy;

  symbol = "AA";
  num_shares = 100;
  price = 50;
  // Submit Order.
  result = trader.SubmitOrder(symbol, &order, order_type, order_action,
                              num_shares, price);

  // * 1.3 Submitting a Sell Order
  // We will place an order as before but we set the order action to
  // cloud_ex.OrderAction.sell.
  order_type = OrderType::limit;
  order_action = OrderAction::sell;

  symbol = "AA";
  num_shares = 100;
  price = 55;

  // Submit Order.
  result = trader.SubmitOrder(symbol, &order, order_type, order_action,
                              num_shares, price);
  std::cout << "Return Order: " << order.SerializeOrder() << std::endl;
  std::string order_id;
  if (result == OrderResult::in_sequencer) {
    order_id = order.order_id_;
  }

  // * 1.4 View Pending Orders
  // You can view all outstanding orders (those that have not been fulfilled) by
  // calling the Trader.GetOutstandingOrders function. Doing so can give you the
  // information needed to cancel any or all orders.

  std::map<std::string, Order> outstanding_orders;
  success = trader.GetOutstandingOrders(&outstanding_orders);

  // * 1.5 Cancelling an Order
  // Given an order ID, we can cancel it if has not been fulfilled.
  // Let's try to cancel the sell order we just placed using the
  // Trader.SubmitCancel function.

  // Arguments to Trader.SubmitCancel():
  //     order_id str - The order id for order to cancel.

  result = trader.SubmitCancel(order_id);
  if (result == OrderResult::in_sequencer) {
    std::cout << "Successfully submitted cancel order: " << order_id
              << std::endl;
  }

  // **************************************************************************
  // * 2. Fetching Real-time Market Data
  // **************************************************************************

  // * 2.1 Subscribe to data for a set of Symbols
  // Before we fetch market data, we will configure our trading api to pull data
  // for a subset of tickers we are interested in.

  // Arguments to Trader.ConfigActiveSymbols:
  //     symbols list - list of tickers str that we will fetch data for
  std::vector<std::string> active_symbols = {"AA", "AB", "AD"};
  success = trader.ConfigActiveSymbols(active_symbols);
  if (success) {
    std::cout << "Successfully configured active symbols" << std::endl;
  }

  // * 2.2 Fetch Recent Trades
  // For any of the symbols we have subscribed to, we can pull recent trades. We
  // will use the Trader.GetRecentTrades to do so.

  // Arguments to Trader.GetRecentTrades():
  //     symbol str - Ticker we will fetch data for.
  //     trade_vec cloud_ex.VectorTrade - Data structure to hold the recent
  //     trades. start_fetch_time int - Time (in unix epoch microseconds) to
  //     start fetching data from.
  symbol = "AA";
  std::vector<Trade> trade_vec;
  uint64_t start_fetch_time = utils::GetMicrosecondTimestamp() - (5 * 1000000);
  success = trader.GetRecentTrades(symbol, &trade_vec, start_fetch_time);
  std::cout << "Get recent trades, total = " << trade_vec.size() << std::endl;

  // * 2.3 Fetch Recent Limit Order Books
  // For any of the symbols we have subscribed to, we can pull recent limit
  // order books. We will use the Trader.GetRecentLOBs to do so.

  // Arguments to Trader.GetRecentLOBs():
  //     symbol str - Ticker we will fetch data for.
  //     trade_vec cloud_ex.VectorLOB - Data structure to hold the recent LOBs.
  //     start_fetch_time int - Time (in unix epoch microseconds) to start
  //     fetching data from.
  symbol = "AA";
  std::vector<LimitOrderBook> lob_vec;
  start_fetch_time = utils::GetMicrosecondTimestamp() - (5 * 1000000);
  success = trader.GetRecentLOBs(symbol, &lob_vec, start_fetch_time);
  std::cout << "Get recent lobs, total = " << lob_vec.size() << std::endl;

  // * 2.4 Fetching Portfolio Matrix
  // We can track the current portfolio matrix using the
  // Trader.GetPortfolioMatrixfunction.
  std::map<std::string, int> portfolio_mat;
  trader.GetPortfolioMatrix(&portfolio_mat);
  std::cout << "Get portfolio" << std::endl;

  // **************************************************************************
  // * 3. Fetching Historical Symbol Market Data
  // **************************************************************************
  symbol = "AA";
  uint64_t start_time_ms = 0;
  uint64_t end_time_ms = 9000000000000000;
  std::vector<Trade> symbol_trades_vec;

  MarketDataAPI::PullTrades(project_id, bigtable_id, table_name, symbol,
                            start_time_ms, end_time_ms, &symbol_trades_vec);
  std::cout << "Pull trades, total = " << symbol_trades_vec.size() << std::endl;

  // **************************************************************************
  // * 4. Fetching Historical Personal Data
  // **************************************************************************

  // * 4.1 Fetching Historical Personal Orders
  // You can view all your historical orders using
  // Trader.GetAllHistoricalOrders. The resulting data can inform which orders
  // you placed and when.
  std::vector<Order> my_historical_orders;
  success = trader.GetAllHistoricalOrders(&my_historical_orders);
  std::cout << "Pull my hist orders, total = " << my_historical_orders.size()
            << std::endl;

  // * 4.2 Fetch Historical Personal Trades
  // You can view all tour historical trades using
  // Trader.GetAllHistoricalTrades. The resulting data can inform which of your
  // orders got transacted and for how much.
  std::vector<Trade> my_historical_trades;
  success = trader.GetAllHistoricalTrades(&my_historical_trades);
  std::cout << "Pull my hist trades, total = " << my_historical_trades.size()
            << std::endl;

  return 0;
}