#ifndef TRADER_MARKET_DATA_API_H_
#define TRADER_MARKET_DATA_API_H_

#include <string>
#include <vector>

#include "common/message_types.h"
#include "common/network_utils.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/table_admin.h"

class MarketDataAPI {
 public:
  // Initializes the ZMQ Communication Channels to Recieve Market Information
  // and provides an abstract class for specific market data APIs.
  MarketDataAPI(const std::string &gateway_ip, const std::string &channel,
                int port);
  ~MarketDataAPI();

  // Queries the gateway for the latest list of symbols that can be traded.
  static bool PullSymbolList(const std::string &gateway_ip,
                             std::vector<std::string> *symbols);

  // Queries the gateway for the latest client information, and returns the
  // client's portfolio along with the orders pending in the book with a delay
  // to ensure that the exchange fairness property is not violated.
  static bool PullClientInformation(const std::string &client_id,
                                    const std::string &gateway_ip,
                                    const std::string &authentication_token,
                                    ClientInformationSnapshot *snapshot);

  static int PullMarketData(const std::string &project_id,
                            const std::string &instance_id,
                            const std::string &table_name,
                            const std::string &col_name,
                            const std::string &row_prefix,
                            uint64_t start_time_ms, uint64_t end_time_ms,
                            std::vector<std::string> *cell_strings);

  static int PullTrades(const std::string &project_id,
                        const std::string &instance_id,
                        const std::string &table_name,
                        const std::string &query_name, uint64_t start_time_ms,
                        uint64_t end_time_ms, std::vector<Trade> *trades);

  static int PullOrders(const std::string &project_id,
                        const std::string &instance_id,
                        const std::string &table_name,
                        const std::string &client_id, uint64_t start_time_ms,
                        uint64_t end_time_ms, std::vector<Order> *order);

 protected:
  void *context_;     // Abstract ZMQ Context (Inherited by Subclasses)
  void *subscriber_;  // Abstract ZMQ Subscriber (Inherited by Subclasses)
  char buffer_[BUFFER_SIZE];  // General Buffer (Inherited by Subclasses)
};

class OrderConfirmationAPI : public MarketDataAPI {
 public:
  OrderConfirmationAPI(const std::string &gateway_ip,
                       const std::string &client_id);

  // Returns the next not-yet-read trade confirmation recieved by the gateway
  // hold/release buffer over the trade confirmation subscription channel.
  bool FetchNextOrderConfirmation(Order *order_confirmation,
                                  OrderResult *order_status);
};

class TradeConfirmationAPI : public MarketDataAPI {
 public:
  TradeConfirmationAPI(const std::string &gateway_ip,
                       const std::string &client_id);

  // Returns the next not-yet-read trade confirmation recieved by the gateway
  // hold/release buffer over the trade confirmation subscription channel.
  bool FetchNextTradeConfirmation(Trade *trade_confirmation);
};

class LimitBookAPI : public MarketDataAPI {
 public:
  LimitBookAPI(const std::string &gateway_ip, const std::string &symbol);

  // Returns the next not-yet-read limit order book recieved by the gateway
  // hold/release buffer over the limit order book subscription channel.
  bool FetchNextLimitBook(LimitOrderBook *limit_order_book);
};

class TradeReportAPI : public MarketDataAPI {
 public:
  TradeReportAPI(const std::string &gateway_ip, const std::string &symbol);

  // Returns the next not-yet-read trade report (anonymized) recieved by the
  //  gateway hold/release buffer over the trade reports subscription channel.
  bool FetchNextTradeReport(Trade *trade_report);
};

#endif  // TRADER_MARKET_DATA_API_H_
