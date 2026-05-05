//+------------------------------------------------------------------+
//|  LedgerBridge.mq5                                                |
//|  Polls for a signal file written by the Ledger app and           |
//|  executes a pending limit order in MT5.                           |
//|                                                                  |
//|  SETUP:                                                          |
//|  1. Copy this file to:  <MT5>\MQL5\Experts\LedgerBridge.mq5     |
//|  2. In Ledger Settings, set the signal file path to:             |
//|       <MT5>\MQL5\Files\ledger_signal.json                        |
//|  3. Attach this EA to any chart. AutoTrading must be ON.         |
//+------------------------------------------------------------------+
#property copyright "Ledger App"
#property version   "1.00"
#property strict

#include <Trade\Trade.mqh>

//--- inputs
input string SignalFileName = "ledger_signal.json"; // Signal file name (inside MQL5/Files/)
input int    CheckEveryMs   = 1000;                 // Polling interval in milliseconds
input ulong  MagicNumber    = 20260427;             // Magic number for orders
input ulong  SlippagePts    = 10;                   // Max slippage in points

CTrade trade;

//+------------------------------------------------------------------+
int OnInit()
{
    trade.SetExpertMagicNumber(MagicNumber);
    trade.SetDeviationInPoints(SlippagePts);
    EventSetMillisecondTimer(CheckEveryMs);
    PrintFormat("LedgerBridge started. Watching: MQL5/Files/%s", SignalFileName);
    return INIT_SUCCEEDED;
}

//+------------------------------------------------------------------+
void OnDeinit(const int reason)
{
    EventKillTimer();
}

//+------------------------------------------------------------------+
void OnTimer()
{
    if (!FileIsExist(SignalFileName)) return;

    int handle = FileOpen(SignalFileName, FILE_READ | FILE_TXT | FILE_ANSI);
    if (handle == INVALID_HANDLE) return;

    int successCount = 0;
    int failCount = 0;
    while (!FileIsEnding(handle))
    {
        string content = FileReadString(handle);
        if (StringLen(content) <= 2) {
            continue;
        }

        // Parse fields from compact JSON object line
        string pair      = JsonStr(content, "pair");
        string direction = JsonStr(content, "direction");
        string account   = JsonStr(content, "account");
        double entry     = JsonDbl(content, "entry");
        double sl        = JsonDbl(content, "sl");
        double tp        = JsonDbl(content, "tp");
        double lotSize   = JsonDbl(content, "lotSize");

        if (pair == "" || (direction != "BUY" && direction != "SELL") ||
            entry <= 0 || sl <= 0 || tp <= 0 || lotSize <= 0)
        {
            Print("LedgerBridge: Invalid signal line - skipping.");
            Print("  Raw content: ", content);
            failCount++;
            continue;
        }

        ENUM_ORDER_TYPE orderType = (direction == "BUY") ? ORDER_TYPE_BUY_LIMIT
                                                         : ORDER_TYPE_SELL_LIMIT;

        bool ok = trade.OrderOpen(pair, orderType, lotSize,
                                  0,
                                  entry, sl, tp,
                                  ORDER_TIME_GTC, 0,
                                  "Ledger " + account);
        if (ok)
        {
            PrintFormat("LedgerBridge: Order placed - %s %s account=%s lots=%.2f entry=%.5f sl=%.5f tp=%.5f",
                        direction, pair, account, lotSize, entry, sl, tp);
            successCount++;
        }
        else
        {
            PrintFormat("LedgerBridge: Order failed - %s (code %u)",
                        trade.ResultRetcodeDescription(), trade.ResultRetcode());
            failCount++;
        }
    }
    FileClose(handle);

    // Delete queue file after processing to prevent duplicates.
    if (!FileDelete(SignalFileName))
        PrintFormat("LedgerBridge: Warning - could not delete %s", SignalFileName);

    PrintFormat("LedgerBridge: Queue processed. Success=%d Failed=%d", successCount, failCount);
}

void OnTick() {}

//+------------------------------------------------------------------+
//| Minimal JSON helpers — no external library required               |
//+------------------------------------------------------------------+

// Returns the string value for a given key, e.g. "pair": "EURUSD"
string JsonStr(const string &json, const string &key)
{
    string search = "\"" + key + "\"";
    int pos = StringFind(json, search);
    if (pos < 0) return "";

    // Find first quote after the colon
    pos = StringFind(json, ":", pos + StringLen(search));
    if (pos < 0) return "";
    pos = StringFind(json, "\"", pos + 1);
    if (pos < 0) return "";

    int end = StringFind(json, "\"", pos + 1);
    if (end < 0) return "";

    return StringSubstr(json, pos + 1, end - pos - 1);
}

// Returns the numeric value for a given key, e.g. "entry": 1.2345
double JsonDbl(const string &json, const string &key)
{
    string search = "\"" + key + "\"";
    int pos = StringFind(json, search);
    if (pos < 0) return 0.0;

    pos = StringFind(json, ":", pos + StringLen(search));
    if (pos < 0) return 0.0;
    pos++;

    // Skip whitespace
    while (pos < StringLen(json))
    {
        ushort ch = StringGetCharacter(json, pos);
        if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') break;
        pos++;
    }

    int start = pos;
    while (pos < StringLen(json))
    {
        ushort ch = StringGetCharacter(json, pos);
        if (!((ch >= '0' && ch <= '9') || ch == '.' || ch == '-')) break;
        pos++;
    }

    if (pos == start) return 0.0;
    return StringToDouble(StringSubstr(json, start, pos - start));
}
