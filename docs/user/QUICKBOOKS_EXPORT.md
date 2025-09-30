# QuickBooks CSV Export Feature

## Overview

ViewTouch now includes a QuickBooks CSV export feature that allows you to export sales data in a format compatible with QuickBooks Online and Desktop for easy import.

## Features

- **Multiple Export Formats**: Daily, Monthly, or Custom date range exports
- **Comprehensive Data**: Includes sales by category, payments, taxes, discounts, tips, and more
- **Configurable Settings**: Customizable export path and automatic export options
- **QuickBooks Compatible**: CSV format designed for seamless QuickBooks import

## CSV Format

The exported CSV files include the following columns:
- **Date**: Transaction date (YYYY-MM-DD format)
- **Type**: Transaction type (Income, Payment, Expense, Discount, Liability, Deposit)
- **Account**: QuickBooks account name
- **Description**: Transaction description
- **Amount**: Transaction amount (decimal format)
- **Tax**: Tax amount (decimal format)

## Data Included

### Sales Data
- Gross sales by category (Food, Beverages, Alcohol, Merchandise, etc.)
- Individual item sales with proper categorization

### Payment Methods
- Cash payments and deposits
- Credit card payments
- Check payments
- Gift certificate payments

### Adjustments
- Discounts applied
- Comps (complimentary items)
- Voids and refunds

### Tax Information
- Sales tax collected
- Tax breakdown by rate

### Tips and Gratuity
- Server tips
- Captured tips
- Tip allocations

## Configuration

### Settings

The following settings are available in the Admin panel:

1. **Export Path**: Directory where CSV files are saved (default: `/usr/viewtouch/exports/quickbooks`)
2. **Auto Export**: Enable automatic daily exports
3. **Export Format**: Choose between Daily, Monthly, or Custom formats

### File Naming Convention

- **Daily**: `quickbooks_YYYYMMDD.csv`
- **Monthly**: `quickbooks_YYYYMM.csv`
- **Custom**: `quickbooks_YYYYMMDD.csv` (based on start date)

## Usage

### Manual Export

1. Navigate to **Admin Panel > Reports**
2. Select **"Export for QuickBooks (CSV)"**
3. Choose date range (Daily, Monthly, or Custom)
4. The system will generate and save the CSV file to the configured export directory

### Programmatic Export

The export can also be triggered programmatically:

```cpp
// Create QuickBooks CSV printer
PrinterQuickBooksCSV *printer = new PrinterQuickBooksCSV("", 0, "/path/to/exports", TARGET_QUICKBOOKS_CSV);

// Set date range
TimeInfo start_time, end_time;
start_time.Set(2024, 1, 1, 0, 0, 0);
end_time.Set(2024, 1, 31, 23, 59, 59);

// Generate export
System::QuickBooksCSVExport(terminal, start_time, end_time, printer);
```

## QuickBooks Import

### QuickBooks Online

1. In QuickBooks Online, go to **Banking** or **Transactions**
2. Click **Import** or **Upload**
3. Select the CSV file from the export directory
4. Map the columns to QuickBooks fields:
   - Date → Transaction Date
   - Type → Transaction Type
   - Account → Account Name
   - Description → Description/Memo
   - Amount → Amount
   - Tax → Tax Amount

### QuickBooks Desktop

1. Go to **File > Utilities > Import > General Journal Entries**
2. Select the CSV file
3. Map the columns as described above
4. Review and import the transactions

## Technical Implementation

### New Components

1. **PrinterQuickBooksCSV**: Specialized printer class for CSV export
2. **TARGET_QUICKBOOKS_CSV**: New target type for QuickBooks exports
3. **QuickBooksCSVExport()**: System method for generating export data
4. **QuickBooks Export Settings**: Configuration options in Settings class

### File Structure

```
main/
├── printer.hh/cc          # Updated with QuickBooks CSV printer
├── system_report.cc       # QuickBooks export implementation
├── system.hh              # Export method declaration
└── settings.hh/cc         # QuickBooks configuration settings

zone/
├── report_zone.hh/cc      # Updated with export UI
└── report_zone_quickbooks.cc  # Export implementation
```

## Troubleshooting

### Common Issues

1. **Export Path Not Found**: Ensure the export directory exists and has proper permissions
2. **Empty CSV Files**: Check that there are closed checks in the specified date range
3. **Permission Errors**: Ensure the ViewTouch process has write access to the export directory

### File Permissions

Make sure the export directory has proper permissions:
```bash
sudo mkdir -p /usr/viewtouch/exports/quickbooks
sudo chown viewtouch:viewtouch /usr/viewtouch/exports/quickbooks
sudo chmod 755 /usr/viewtouch/exports/quickbooks
```

## Future Enhancements

- Support for additional QuickBooks field mappings
- Automatic email delivery of export files
- Integration with QuickBooks API for direct import
- Support for multiple export formats (IIF, QIF)
- Scheduled automatic exports with cron integration
