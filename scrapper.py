import yfinance as yf
import pandas as pd
import time
import os
from datetime import datetime

# --- CONFIGURATION ---
TICKERS = ["SPY", "AAPL", "TSLA", "QQQ", "NVDA"]
RATES_SYMBOL = "^IRX"
# ---------------------

def save_to_ticker_folder(df, ticker, filename):
    folder_path = os.path.join('data', ticker.replace("^", ""))
    os.makedirs(folder_path, exist_ok=True)
    file_path = os.path.join(folder_path, filename)
    
    if not os.path.exists(file_path):
        df.to_csv(file_path, index=False)
        print(f"  [NEW] {file_path}")
    else:
        existing_df = pd.read_csv(file_path)
        combined_df = pd.concat([existing_df, df], ignore_index=True)
        
        if 'timestamp' in combined_df.columns:
            combined_df = combined_df.drop_duplicates(subset=['timestamp'])
        else:
            combined_df = combined_df.drop_duplicates()
            
        combined_df.to_csv(file_path, index=False)
        print(f"  [UPDATED] {file_path} (Total rows: {len(combined_df)})")

def fetch_data_for_ticker(symbol):
    print(f"\n--- Fetching: {symbol} ---")
    ticker_obj = yf.Ticker(symbol)
    current_time_unix = int(time.time())

    # 1. Spot Prices
    spot_history = ticker_obj.history(period="730d", interval="1h")
    if not spot_history.empty:
        spot_history = spot_history.dropna(subset=['Close']) # Drop empty rows first
        
        spot_df = pd.DataFrame()
        spot_df['timestamp'] = spot_history.index.astype('int64') // 10**9
        
        # FIX: Added .values to ignore the index and just grab the raw numbers
        spot_df['close'] = spot_history['Close'].values.round(2) 
        
        save_to_ticker_folder(spot_df, symbol, 'spot_prices.csv')

    # 2. Option Surface
    try:
        options_data = []
        expirations = ticker_obj.options[:3] 
        for exp in expirations:
            opt_chain = ticker_obj.option_chain(exp)
            exp_date = datetime.strptime(exp, '%Y-%m-%d').timestamp()
            maturity = round((exp_date - current_time_unix) / (365.25 * 24 * 3600), 4)

            for opt_type, chain in [('CALL', opt_chain.calls), ('PUT', opt_chain.puts)]:
                for _, row in chain.iterrows():
                    if row['bid'] > 0 and row['ask'] > 0:
                        options_data.append([
                            current_time_unix, opt_type, row['strike'], 
                            maturity, round(row['bid'], 2), round(row['ask'], 2),
                            round(row['impliedVolatility'], 4)
                        ])
        
        if options_data:
            cols = ['fetch_timestamp', 'type', 'strike', 'maturity', 'bid', 'ask', 'implied_vol']
            # No .values needed here because we are building a clean list of lists
            save_to_ticker_folder(pd.DataFrame(options_data, columns=cols), symbol, 'option_surface.csv')
    except Exception as e:
        print(f"  [!] No options for {symbol}: {e}")

def fetch_rates():
    print(f"\n--- Fetching Rates: {RATES_SYMBOL} ---")
    rates_ticker = yf.Ticker(RATES_SYMBOL)
    rates_history = rates_ticker.history(period="60d")
    
    if not rates_history.empty:
        rates_history = rates_history.dropna(subset=['Close'])
        
        rates_df = pd.DataFrame()
        rates_df['timestamp'] = rates_history.index.astype('int64') // 10**9
        
        # FIX: Added .values here as well
        rates_df['rate'] = (rates_history['Close'].values / 100).round(4)
        
        save_to_ticker_folder(rates_df, "RATES", 'rates.csv')

if __name__ == "__main__":
    for symbol in TICKERS:
        fetch_data_for_ticker(symbol)
    fetch_rates()
    print("\nScrape complete.")
