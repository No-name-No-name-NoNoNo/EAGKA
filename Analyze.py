import glob
import os
import sqlite3
import pandas as pd
from datetime import datetime

def merge_csv_files():
    """Merge CSV files"""
    input_files = sorted(glob.glob("results_cache/*.csv"))
    # Filename is timestamp_Result.csv
    # Get current timestamp
    timestamp = datetime.now().strftime("%Y%m%d%H%M%S")
    output_file = timestamp + "_Result.csv"

    with open(output_file, "w") as fout:
        for fname in input_files:
            with open(fname, "r") as fin:
                for line in fin:
                    line = line.strip()
                    if not line:
                        continue  # Skip empty lines
                    fout.write(line + "\n")

    # Clear results_cache directory after merging
    for fname in input_files:
        os.remove(fname)

    print("Merged %d files to %s" % (len(input_files), output_file))
    
    return output_file

def create_database(csv_file=None, db_file=None):
    """Convert CSV file to SQLite database"""
    if csv_file is None:
        # If no CSV file specified, use the latest one
        result_files = sorted(glob.glob("*_Result.csv"), reverse=True)
        if not result_files:
            print("Error: Result CSV file not found")
            return None
        csv_file = result_files[0]
    
    # If no database filename specified, generate timestamp filename
    if db_file is None:
        timestamp = datetime.now().strftime("%Y%m%d%H%M%S")
        db_file = timestamp + "_experiment_results.db"
    
    if not os.path.exists(csv_file):
        print("Error: CSV file '{}' does not exist".format(csv_file))
        return None

    print("Reading CSV file: {}".format(csv_file))
    # Define column names to match output format based on original code
    columns = [
        'timestamp', 'areaLength', 'areaWidth', 'areaHeight', 'numNodes', 'linkQuality', 
        'runId', 'keyAgreementDelay', 'totalSent', 'totalReceived', 'overheadRatio', 'successRate', 'avgUniqueContributions'
    ]
    
    try:
        # Read CSV (no header), use defined column names
        df = pd.read_csv(csv_file, header=None, names=columns)
        print("Successfully read {} rows of data".format(len(df)))
        
        # Remove timestamp and runId columns
        df = df.drop(['timestamp', 'runId'], axis=1)
        print("Removed timestamp and runId columns")
        
        # Connect to SQLite database
        conn = sqlite3.connect(db_file)
        print("Connected to database: {}".format(db_file))
        
        # Create table
        conn.execute('''
        CREATE TABLE IF NOT EXISTS experiment_results (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            areaLength INTEGER,
            areaWidth INTEGER,
            areaHeight INTEGER,
            numNodes INTEGER,
            linkQuality TEXT,
            keyAgreementDelay REAL,
            totalSent INTEGER,
            totalReceived INTEGER,
            overheadRatio REAL,
            successRate REAL,
            avgUniqueContributions REAL
        )
        ''')
        
        # Insert data
        df.to_sql('experiment_results', conn, if_exists='replace', index=False)
        
        # Verify data insertion
        count = conn.execute("SELECT COUNT(*) FROM experiment_results").fetchone()[0]
        print("Successfully inserted {} records to database".format(count))
        
        # Close connection
        conn.close()
        print("Database conversion completed")
        
        return db_file
    except Exception as e:
        print("Error: {}".format(e))
        return None

def analyze_database(db_file=None):
    """Analyze database and generate Excel report"""
    if db_file is None:
        # If no database file specified, use the latest one
        db_files = sorted(glob.glob("*_experiment_results.db"), reverse=True)
        if not db_files:
            db_file = 'experiment_results.db'  # Use default filename
        else:
            db_file = db_files[0]
    
    if not os.path.exists(db_file):
        print("Error: Database file '{}' does not exist".format(db_file))
        return False
        
    print("Starting database analysis: {}".format(db_file))
    
    # Generate timestamp Excel filename
    timestamp = datetime.now().strftime("%Y%m%d%H%M%S")
    excel_file = timestamp + "_analysis_results.xlsx"
    
    # Connect database
    conn = sqlite3.connect(db_file)
    
    # Create Excel writer
    excel_writer = pd.ExcelWriter(excel_file, engine='openpyxl')
    
    
    # 2. Delay analysis
    delay_analysis = pd.read_sql_query("""
        SELECT 
            linkQuality AS LinkQuality,
            areaLength || '*' || areaWidth || '*' || areaHeight AS AreaSize,
            numNodes AS NodeCount,
            COUNT(*) AS TotalCount,
            SUM(CASE WHEN keyAgreementDelay > 0 AND successRate = 100 THEN 1 ELSE 0 END) AS SuccessCount,
            ROUND(AVG(CASE WHEN keyAgreementDelay > 0 AND successRate = 100 THEN keyAgreementDelay ELSE NULL END), 4) AS AvgDelay,
            ROUND(MIN(CASE WHEN keyAgreementDelay > 0 AND successRate = 100 THEN keyAgreementDelay ELSE NULL END), 4) AS MinDelay,
            ROUND(MAX(CASE WHEN keyAgreementDelay > 0 AND successRate = 100 THEN keyAgreementDelay ELSE NULL END), 4) AS MaxDelay
        FROM experiment_results
        GROUP BY linkQuality, areaLength, areaWidth, areaHeight, numNodes
        ORDER BY 
            CASE
                WHEN linkQuality = 'high' THEN 1
                WHEN linkQuality = 'medium' THEN 2
                WHEN linkQuality = 'low' THEN 3
                WHEN linkQuality = 'very_poor' THEN 4
                ELSE 5
            END,
            CASE
                WHEN areaLength || '*' || areaWidth || '*' || areaHeight = '300*300*80' THEN 1
                WHEN areaLength || '*' || areaWidth || '*' || areaHeight = '500*500*150' THEN 2
                WHEN areaLength || '*' || areaWidth || '*' || areaHeight = '800*800*200' THEN 3
                WHEN areaLength || '*' || areaWidth || '*' || areaHeight = '1000*1000*300' THEN 4
                ELSE 5
            END,
            numNodes
    """, conn)
    delay_analysis.to_excel(excel_writer, sheet_name=u'Delay Analysis', index=False)
    
    # 3. Packet statistics
    packet_stats = pd.read_sql_query("""
        SELECT 
            linkQuality AS LinkQuality,
            areaLength || '*' || areaWidth || '*' || areaHeight AS AreaSize,
            numNodes AS NodeCount,
            COUNT(*) AS TotalCount,
            CAST(ROUND(AVG(totalSent) + 0.5) AS INTEGER) AS AvgSentPackets,
            CAST(ROUND(AVG(totalReceived) + 0.5) AS INTEGER) AS AvgReceivedPackets,
            ROUND(AVG(avgUniqueContributions), 2) AS AvgUniqueContributions
        FROM experiment_results
        GROUP BY linkQuality, areaLength, areaWidth, areaHeight, numNodes
        ORDER BY 
            CASE
                WHEN linkQuality = 'high' THEN 1
                WHEN linkQuality = 'medium' THEN 2
                WHEN linkQuality = 'low' THEN 3
                WHEN linkQuality = 'very_poor' THEN 4
                ELSE 5
            END,
            CASE
                WHEN areaLength || '*' || areaWidth || '*' || areaHeight = '300*300*80' THEN 1
                WHEN areaLength || '*' || areaWidth || '*' || areaHeight = '500*500*150' THEN 2
                WHEN areaLength || '*' || areaWidth || '*' || areaHeight = '800*800*200' THEN 3
                WHEN areaLength || '*' || areaWidth || '*' || areaHeight = '1000*1000*300' THEN 4
                ELSE 5
            END,
            numNodes
    """, conn)
    packet_stats.to_excel(excel_writer, sheet_name=u'Packet Statistics', index=False)
    
    # 4. Success rate analysis
    success_analysis = pd.read_sql_query("""
        SELECT 
            linkQuality AS LinkQuality,
            areaLength || '*' || areaWidth || '*' || areaHeight AS AreaSize,
            numNodes AS NodeCount,
            COUNT(*) AS TotalCount,
            SUM(CASE WHEN keyAgreementDelay > 0 THEN 1 ELSE 0 END) AS SuccessCount,
            ROUND(SUM(CASE WHEN keyAgreementDelay > 0 THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 2) AS SuccessRate
        FROM experiment_results
        GROUP BY linkQuality, areaLength, areaWidth, areaHeight, numNodes
        ORDER BY 
            CASE
                WHEN linkQuality = 'high' THEN 1
                WHEN linkQuality = 'medium' THEN 2
                WHEN linkQuality = 'low' THEN 3
                WHEN linkQuality = 'very_poor' THEN 4
                ELSE 5
            END,
            CASE
                WHEN areaLength || '*' || areaWidth || '*' || areaHeight = '300*300*80' THEN 1
                WHEN areaLength || '*' || areaWidth || '*' || areaHeight = '500*500*150' THEN 2
                WHEN areaLength || '*' || areaWidth || '*' || areaHeight = '800*800*200' THEN 3
                WHEN areaLength || '*' || areaWidth || '*' || areaHeight = '1000*1000*300' THEN 4
                ELSE 5
            END,
            numNodes
    """, conn)
    success_analysis.to_excel(excel_writer, sheet_name=u'Success Rate Analysis', index=False)

    excel_writer.close()
    conn.close()    
    return True



def do_all_steps():
    """Execute all steps: merge CSV, convert to database, analyze"""
    print("===== Starting full process =====")
    csv_file = merge_csv_files()
    if csv_file:
        print("\n===== CSV merge completed, starting database conversion =====")
        db_file = create_database(csv_file)
        if db_file:
            print("\n===== Database conversion completed, starting data analysis =====")
            analyze_database(db_file)
            print("\n===== Full process completed =====")

def main():
    do_all_steps()

if __name__ == "__main__":
    main()

