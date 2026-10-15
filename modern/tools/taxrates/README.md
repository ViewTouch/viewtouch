# Sales tax rates by ZIP code

`build.py` makes `seed/tax/areas.json`: the sales tax rate in every US ZIP code, from each state's own published rates, and each state's restaurant rules (`data/states.json`). ViewTouch suggests the rate for the store's ZIP code; the manager confirms it.

Refresh it every quarter (rates change January 1, April 1, July 1 and October 1): download the sources below into `~/.cache/vtm-taxsrc` (outside the repository: hundreds of MB), run `python3 -I build.py --on YYYYMMDD` (the date the rates should be in effect), check the spot-check list, and commit `seed/tax/areas.json`.

## Sources

| State | From | Saved as |
|---|---|---|
| AR GA IA IN KS KY MI MN NC ND NE NJ NV OH OK RI SD TN UT VT WA WI WV WY | Streamlined Sales Tax rate and boundary files, <https://streamlinedsalestax.org/ratesandboundry/Rates/> and `/Boundary/` (unzip all into one folder) | `sst/x/` |
| All | Census 2020 county and place names, <https://www2.census.gov/geo/docs/reference/codes2020/> (`national_county2020.txt`, `national_place2020.txt`); ZIP areas to counties and places, <https://www2.census.gov/geo/docs/maps-data/data/rel2020/zcta520/> (`tab20_zcta520_county20_natl.txt`, `tab20_zcta520_place20_natl.txt`) | `census/` |
| CA | CDTFA, <https://www.cdtfa.ca.gov/taxes-and-fees/sales-use-tax-rates.htm> (the .xlsx, converted to `rates.csv` with LibreOffice) | `states/CA/` |
| TX | Comptroller's EDI rate file, <https://comptroller.texas.gov/taxes/file-pay/edi/sales-tax-rates.php> (`taxrates.txt`) | `states/TX/` |
| MO | DOR rate tables, <https://dor.mo.gov/business/sales/rates/> (the quarter's PDF, `pdftotext -layout` to `oct2026.txt`; rename in `states_mo.py`) | `states/MO/` |
| VA | Virginia Tax, rates by locality, <https://www.tax.virginia.gov/retail-sales-and-use-tax> (.xlsx, every sheet to CSV: `rates-*.csv`) | `states/VA/` |
| NM | TRD gross receipts tax locations (its public map data), `New_Mexico_gross_receipts_tax_<period>` on services5.arcgis.com, saved as `locations.json` | `states/NM/` |
| AL | DOR, `taxrates_current.csv` from <https://revenue.alabama.gov/> | `states/AL/` |
| AZ | DOR TPT rate table, all business codes, <https://azdor.gov/node/831> (`rates.csv`; restaurants and bars, code 011) | `states/AZ/` |
| CO | DOR jurisdiction codes and rates, <https://tax.colorado.gov/how-to-look-up-sales-use-tax-rates> (.xlsx, every sheet to CSV) | `states/CO/` |
| AK | Remote Seller Sales Tax Commission rate sheet with ZIP codes, <https://arsstc.org/business-sellers/tax-rates/> (.xlsx, every sheet to CSV) | `states/AK/` |
| IL | DOR machine-readable files, <https://tax.illinois.gov/research/taxrates/sales-tax-rate-machine-readable-files.html> (`ordmache-current.txt` as `ordmache.txt`, `ordmacha-current.txt` as `ordmacha.txt`) | `states/IL/` |
| NY FL SC PA HI MS | Hand-checked tables in `data/`, each naming its source and date | `data/*.json` |
| CT DE DC ME MD MA NH OR | One rate statewide (`data/states.json`) | |
| LA ID MT | The state's rate; the app links to the official lookup for the local part (no complete public file) | |

`data/states.json` has each state's rate, what's different for restaurants (meals and alcohol rates that replace or add to the sales tax), and notes on local restaurant taxes that aren't in the rate.

## How a ZIP code gets its rate

- Streamlined states: the rate covering most of the ZIP's addresses (or ZIP+4 codes); the others are listed as choices.
- The others: the Census says which county and cities each ZIP area lies in. A city with its own rate that covers a tenth of the ZIP's land or more is the default (restaurants are mostly in town); the county's rate (outside cities) and any other city's are listed as choices. The manager picks the one for the store's address.
