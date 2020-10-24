#include <algorithm>
#include <mutex>

#include "trader/trader_api.h"

#define HIGHEST_SELL_PRICE 99999999
#define LOWEST_BUY_PRICE 0

// Google Command Flags

/* Setup and identity flags */
DEFINE_string(configuration_path, "/root/vm_config.json",
              "Read your configuration file from this path");
DEFINE_int32(base_shares, 5000, "The base shares for momentum traders");
DEFINE_int32(moving_window, 5, "The window length (seconds) (for momentum)");
DEFINE_int32(tick_length, 1,
             "The basic time unit for moving window (seconds), that means, "
             "after how much time should we record one point of stock price");
DEFINE_double(threshold, 2, "The threshold as a percent (for momentum)");
DEFINE_double(p1, .5, "Weight for previous timestep for momentum traders");
DEFINE_double(p2, .5, "Weight for two timesteps ago for momentum traders");

// Continuous Execution Indicator
static volatile bool run = true;

// Catch CTRL-C Exception and Stop Execution
void SignalHandler(int signal) { run = false; }
// Get symbols list
std::vector<std::string> symbol_list;

double GetHighestBuyPrice(std::vector<LimitOrderBook> *lob) {
  // Only Consider the highest buy price
  double highest_buy_price = LOWEST_BUY_PRICE;
  for (size_t i = 0; i < 1; i++) {
    if (i >= lob->size()) {
      return highest_buy_price;
    }
    for (auto p : (*lob)[i].buy_queue_) {
      if (highest_buy_price < (p.second).limit_price_) {
        highest_buy_price = (p.second).limit_price_;
      }
    }
  }
  return highest_buy_price;
}

double GetLowestSellPrice(std::vector<LimitOrderBook> *lob) {
  // Only Consider the lowest sell price
  double lowest_sell_price = HIGHEST_SELL_PRICE;
  for (size_t i = 0; i < 1; i++) {
    if (i >= lob->size()) {
      return lowest_sell_price;
    }
    for (auto p : (*lob)[i].sell_queue_) {
      if (lowest_sell_price > (p.second).limit_price_) {
        lowest_sell_price = (p.second).limit_price_;
      }
    }
  }
  return lowest_sell_price;
}

void MomentumFunc(Trader *trader_api, std::string target_symbol,
                  uint32_t moving_window_size, uint32_t tick_length,
                  double threshold, int base_shares) {
  std::vector<LimitOrderBook> recent_lobs;
  uint64_t start_timestamp = utils::GetMicrosecondTimestamp();
  uint64_t last_seen_timestamp = 1;
  std::vector<double> stock_prices;
  double latest_stock_price = 1;
  std::queue<Order> previous_orders;
  while (run) {
    while (utils::GetMicrosecondTimestamp() <
           start_timestamp + tick_length * 1000 * 1000) {
      usleep(100);
    }
    start_timestamp = utils::GetMicrosecondTimestamp();
    VLOG(1) << "New Loop StartTimestamp =" << start_timestamp;
    // The top one is the most recent one
    trader_api->GetRecentLOBs(target_symbol, &recent_lobs, last_seen_timestamp);
    if (recent_lobs.size() > 0) {
      last_seen_timestamp = recent_lobs[0].creation_timestamp_;
      // Take the highest buy price in the most recent lob as the stock price
      double highest_buy_price = GetHighestBuyPrice(&recent_lobs);
      double lowest_sell_price = GetLowestSellPrice(&recent_lobs);
      double current_stock_price = highest_buy_price;
      if (current_stock_price > LOWEST_BUY_PRICE) {
        if (stock_prices.size() >= moving_window_size) {
          // Place Order
          double sum_price = 0;
          for (auto e : stock_prices) {
            sum_price += e;
          }
          double average_price = sum_price / stock_prices.size();

          // Calculate aggregate momentum from past two timesteps in series.
          size_t vector_size = stock_prices.size();
          double p1_momentum =
              ((stock_prices[vector_size - 2] - stock_prices[vector_size - 1]) /
               stock_prices[vector_size - 1]) *
              FLAGS_p1;
          double p2_momentum =
              ((stock_prices[vector_size - 3] - stock_prices[vector_size - 2]) /
               stock_prices[vector_size - 2]) *
              FLAGS_p2;
          double agg_momentum = (p1_momentum + p2_momentum) * 100;
          VLOG(1) << "agg_momentum=" << agg_momentum;
          Order ord;
          if (agg_momentum < -threshold &&
              lowest_sell_price < HIGHEST_SELL_PRICE) {
            // Sell
            int num_shares = static_cast<int>(current_stock_price /
                                              average_price * base_shares);
            LOG(ERROR) << start_timestamp
                       << ": Sell Triggered: " << target_symbol << "\t"
                       << num_shares << "\t Current Price"
                       << current_stock_price << "\t AvgPrice" << average_price;
            trader_api->SubmitOrder(target_symbol, &ord, OrderType::limit,
                                    OrderAction::sell, num_shares,
                                    lowest_sell_price - 1);
            LOG(ERROR) << "Submitted selling Order " << ord.SerializeOrder();
            if (ord.order_id_ != "NULL") {
              previous_orders.push(ord);
            }
          } else if (agg_momentum > threshold) {
            int num_shares = static_cast<int>(
                average_price / current_stock_price * base_shares);
            LOG(ERROR) << start_timestamp
                       << ": Buy Triggered: " << target_symbol << "\t"
                       << num_shares << "\t Current Price"
                       << current_stock_price << "\t AvgPrice" << average_price;
            // Buy
            trader_api->SubmitOrder(target_symbol, &ord, OrderType::limit,
                                    OrderAction::buy, num_shares,
                                    highest_buy_price + 1);
            LOG(ERROR) << "Submitted buying Order " << ord.SerializeOrder();
            if (ord.order_id_ != "NULL") {
              previous_orders.push(ord);
            }
          }
        }
        stock_prices.push_back(current_stock_price);
        while (stock_prices.size() > moving_window_size) {
          stock_prices.erase(stock_prices.begin());
        }
        latest_stock_price = current_stock_price;
      } else {
        // currently the lob is empty, so use the latest stock price (>0) as the
        // current stock price
        VLOG(1) << target_symbol << "Order Empty in this LOB";
        stock_prices.push_back(latest_stock_price);
        while (stock_prices.size() > moving_window_size) {
          stock_prices.erase(stock_prices.begin());
        }
      }

    } else {
      VLOG(1) << target_symbol << " LOB Empty-1";
      stock_prices.push_back(latest_stock_price);
      while (stock_prices.size() > moving_window_size) {
        stock_prices.erase(stock_prices.begin());
      }
      VLOG(1) << target_symbol << " Controle price vec size";
    }
    if (previous_orders.size() == 30) {
      for (int i = 0; i < 10; i++) {
        Order ord = previous_orders.front();
        trader_api->SubmitCancel(ord.order_id_);
        VLOG(1) << "i= " << i << "\tCancel " << ord.order_id_;
        previous_orders.pop();
        VLOG(1) << "i= " << i << "\tAfter Pop " << ord.order_id_;
      }
    }
  }
}

int main(int argc, char **argv) {
  // GFLAGS and GLOG Parsing
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  FLAGS_logtostderr = 1;

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

  // Start Trader API
  Trader *trader_api = new Trader(gateway_ip, client_id, client_token);

  std::vector<std::string> target_symbols;
  target_symbols.push_back("AA");
  std::vector<std::thread *> momentum_threads(target_symbols.size());
  trader_api->ConfigActiveSymbols(target_symbols);

  for (int i = 0; i < target_symbols.size(); i++) {
    momentum_threads[i] = new std::thread(
        MomentumFunc, trader_api, target_symbols[i], FLAGS_moving_window,
        FLAGS_tick_length, FLAGS_threshold, FLAGS_base_shares);
  }
  for (int i = 0; i < target_symbols.size(); i++) {
    momentum_threads[i]->join();
  }

  delete trader_api;
}
