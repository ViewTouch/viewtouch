#include "report_zone.hh"
#include "printer.hh"
#include "system.hh"

/*********************************************************************
 * QuickBooksExport:  Export report data to QuickBooks CSV format
 ********************************************************************/
SignalResult ReportZone::QuickBooksExport(Terminal *term)
{
    FnTrace("ReportZone::QuickBooksExport()");
    Employee *e = term->user;
    if (e == NULL)
        return SIGNAL_IGNORED;
    
    // Create QuickBooks CSV printer using settings path
    Settings *settings = term->GetSettings();
    PrinterQuickBooksCSV *qb_printer = new PrinterQuickBooksCSV("", 0, settings->quickbooks_export_path.Value(), TARGET_QUICKBOOKS_CSV);
    
    if (qb_printer == NULL)
    {
        // Use printf for error message since Terminal doesn't have Error method
        printf("Failed to create QuickBooks export printer\n");
        return SIGNAL_IGNORED;
    }
    
    // Set up date range
    TimeInfo start_time = day_start;
    TimeInfo end_time = day_end;
    
    if (!start_time.IsSet())
    {
        start_time.Set();
        start_time -= date::days(1);
        start_time.Floor<date::days>();
    }
    
    if (!end_time.IsSet())
    {
        end_time.Set();
        end_time.Floor<date::days>();
        end_time -= std::chrono::seconds(1);
    }
    
    // Generate QuickBooks CSV export
    int result = term->system_data->QuickBooksCSVExport(term, start_time, end_time, qb_printer);
    
    if (result == 0)
    {
        printf("QuickBooks CSV export completed successfully\n");
    }
    else
    {
        printf("QuickBooks CSV export failed\n");
    }
    
    delete qb_printer;
    return SIGNAL_OKAY;
}
