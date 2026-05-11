//+------------------------------------------------------------------+
//| MT5PriceExporter.mq5                                              |
//| Exports market ticks as JSONL into a FILE_COMMON file for        |
//| external bridge processes (Python sender) to consume.            |
//+------------------------------------------------------------------+
#property strict
#property version "1.00"

input string OutputFileName = "ledger_ticks.jsonl";      // FILE_COMMON target
input string SymbolsCSV = "EURUSD,GBPUSD,USDJPY,USDCHF,USDCAD,AUDUSD,NZDUSD,EURGBP,EURJPY,EURCHF,EURCAD,EURAUD,EURNZD,GBPJPY,GBPCHF,GBPCAD,GBPAUD,GBPNZD,AUDJPY,AUDCHF,AUDCAD,AUDNZD,NZDJPY,NZDCHF,NZDCAD,CADJPY,CADCHF,CHFJPY,XAUUSD,XAGUSD,NAS100,US30,BTCUSD,ETHUSD"; // Symbols to publish
input int    PublishEveryMs = 500;                        // Timer interval

string g_symbols[];

//+------------------------------------------------------------------+
int OnInit()
{
    ParseSymbols(SymbolsCSV);
    if (ArraySize(g_symbols) == 0)
    {
        Print("MT5PriceExporter: No symbols configured.");
        return INIT_FAILED;
    }

    EventSetMillisecondTimer(PublishEveryMs);
    PrintFormat("MT5PriceExporter started. Writing to COMMON/%s", OutputFileName);
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
    for (int i = 0; i < ArraySize(g_symbols); i++)
    {
        string sym = g_symbols[i];

        double bid = 0.0;
        double ask = 0.0;
        if (!SymbolInfoDouble(sym, SYMBOL_BID, bid)) continue;
        if (!SymbolInfoDouble(sym, SYMBOL_ASK, ask)) continue;
        if (bid <= 0.0 && ask <= 0.0) continue;

        long ts_ms = (long)TimeGMT() * 1000;
        string json = StringFormat(
            "{\"symbol\":\"%s\",\"bid\":%.10f,\"ask\":%.10f,\"ts_ms\":%I64d}",
            sym, bid, ask, ts_ms
        );

        AppendLine(json);
    }
}

void OnTick() {}

//+------------------------------------------------------------------+
void ParseSymbols(string csv)
{
    string clean = csv;
    StringReplace(clean, " ", "");
    int count = StringSplit(clean, ',', g_symbols);
    if (count <= 0)
    {
        ArrayResize(g_symbols, 0);
        return;
    }

    int writeIdx = 0;
    for (int i = 0; i < count; i++)
    {
        string sym = StringTrim(g_symbols[i]);
        if (sym == "") continue;
        g_symbols[writeIdx] = StringToUpper(sym);
        writeIdx++;
    }
    ArrayResize(g_symbols, writeIdx);
}

//+------------------------------------------------------------------+
void AppendLine(string line)
{
    int h = FileOpen(OutputFileName, FILE_COMMON | FILE_READ | FILE_WRITE | FILE_TXT | FILE_ANSI);
    if (h == INVALID_HANDLE)
    {
        PrintFormat("MT5PriceExporter: FileOpen failed for %s", OutputFileName);
        return;
    }

    FileSeek(h, 0, SEEK_END);
    FileWriteString(h, line + "\n");
    FileClose(h);
}

//+------------------------------------------------------------------+
string StringTrim(string s)
{
    StringTrimLeft(s);
    StringTrimRight(s);
    return s;
}
