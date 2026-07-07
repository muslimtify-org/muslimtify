```bash
muslimtify show [--json] [--headless]
  --date <date> [--json] [--headless]
  --date <start> --to <end> [--json] [--headless]
  --next

muslimtify location [--json] [--headless]
    set [--lat] [--long] [--timezone] [--city] [--country] [--auto]

muslimtify notification [enable/disable]
    --urgency [normal/critical/low]
    --reminder [--all] <prayer-name> <list minutes before>
    --adhan [enable <prayer>] [disable <prayer>] [set <path>]

muslimtify method <method>
muslimtify madzhab <name>

```
```
```

## `muslimtify show`

Show today's prayer time is ascii table presentation

+------------+----------+----------+-----------------------+
| Prayer     | Time     | Status   | Reminders             |
+------------+----------+----------+-----------------------+
| Fajr       | 04:44    | Enabled  | 30, 15, 5 min before  |
| Sunrise    | 06:03    | Disabled | -                     |
| Dhuha      | 06:27    | Disabled | -                     |
| Dhuhr      | 12:00    | Enabled  | 30, 15, 5 min before  |
| Asr        | 15:22    | Enabled  | 30, 15, 5 min before  |
|>Maghrib    | 17:53    | Enabled  | 30, 15, 5 min before  |
| Isha       | 19:08    | Enabled  | 30, 15, 5 min before  |
+------------+----------+----------+-----------------------+

### `--json`

Usage: `muslimtify show --json` - show today's prayer time in json presentation

```json
{
  "prayers": {
    "fajr": {
      "time": "04:44",
      "enabled": true,
      "reminders": [30, 15, 5]
    },
    "sunrise": {
      "time": "06:03",
      "enabled": false,
      "reminders": []
    },
    "dhuha": {
      "time": "06:27",
      "enabled": false,
      "reminders": []
    },
    "dhuhr": {
      "time": "12:00",
      "enabled": true,
      "reminders": [30, 15, 5]
    },
    "asr": {
      "time": "15:22",
      "enabled": true,
      "reminders": [30, 15, 5]
    },
    "maghrib": {
      "time": "17:53",
      "enabled": true,
      "reminders": [30, 15, 5]
    },
    "isha": {
      "time": "19:08",
      "enabled": true,
      "reminders": [30, 15, 5]
    }
  }
}
```

Note that you cannot use this flag with `--headless` together

### `--headless`

Usage: `muslimtify show --headless` show today's prayer time in plain key=value presentation

```bash
fajr=04:44
dhuhr=12:00
asr=15:22
maghrib=17:53
isha=19:08
```

Note that you cannot use this flat together with `--json` flag

### `--date`

Show prayer time on desire date with date format dd/mm/yyyy

 - Usage: `muslimtify show --date <date>`, show prayer time in desire date for example `muslimtify show --date 07/07/2026` will show prayer time on 07 july 2026, can be show using `--json` or `--headless`, by default using table presentation
 - Usage: `muslimtify show --date <start> --to <end>` is show prayer time from start date to end date, for example `muslimtify show --date 07/07/2026 --to 08/08/2026`, can be show using `--json` or `--headless` default is table presentation:

+-----------------------------------------------------------------------------+
| Date       | Fajr   | Sunrise | Dhuha  | Dhuhr  | Asr    | Maghrib | Isha   |
+----------------------------------------------------------------------+
| 07/07/2026 | <time> | <time>  | <time> | <time> | <time> | <time>  | <time> |
...
| 08/08/2026 | <time> | <time>  | <time> | <time> | <time> | <time>  | <time> |
+-----------------------------------------------------------------------------+

### `--next`

- Usage: `muslimtify show --next` - Show next prayer time

```bash
Isha=19:08
Remaining=00:39
```

## `muslimtify location`

Show location information from current muslimtify config

+------------------------------+
| Name       | Value           |
+------------------------------+
| corditates | -6.2146,106.845 |
| city       | Jakarta         |
| country    | ID              |
| timezone   | Asia/Jakarta    |
| gmt        | UTC+7.0         |
+------------------------------+

### `--json`

Show location information on json format

```json
{
  "coordinates": "-6.2146,106.845",
  "city": "Jakarta",
  "country": "ID",
  "timezone": "Asia/Jakarta",
  "gmt": "UTC+7.0"
}
```

### `--headless`

Show location information in plain key=value format

```bash
cordinates=-6.2146, 106.8451
city=Jakarta
country=ID
timezone=Asia/Jakarta
gmt=UTC+7.0
```

