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

### `set`

Update your saved location. Pass only the fields you want to change — anything you leave out keeps its current value.

- `--lat <latitude>` / `--long <longitude>` — set coordinates by hand, e.g. `muslimtify location set --lat=-6.2146 --long=106.8451`
- `--timezone <iana>` — set the IANA timezone, e.g. `--timezone=Asia/Jakarta`
- `--city <name>` — set the city label shown in `location`, e.g. `--city=Jakarta`
- `--country <iso2>` — set the ISO-2 country code, e.g. `--country=ID`
- `--auto` — figure everything out for you (coordinates, city, country, timezone) from your IP address: `muslimtify location set --auto`

Flags combine: `muslimtify location set --lat=-6.2146 --long=106.8451 --timezone=Asia/Jakarta --city=Jakarta --country=ID`

## `muslimtify notification`

Control whether and how muslimtify notifies you at prayer times. With no argument it prints the current notification settings.

### `enable` / `disable`

Turn prayer-time notifications on or off.

- `muslimtify notification enable` — start showing notifications
- `muslimtify notification disable` — stop showing them

### `--urgency <normal|critical|low>`

How insistent the notification is. `critical` stays on screen until you dismiss it; `normal` and `low` follow the system's usual timeout.

- Usage: `muslimtify notification --urgency critical`

### `--reminder [--all] <prayer-name> <minutes...>`

Add "pre-prayer" reminders — extra notifications a few minutes *before* a prayer.

- `muslimtify notification --reminder fajr 30 15 5` — remind 30, 15, and 5 minutes before Fajr
- `muslimtify notification --reminder --all 10` — set a single 10-minute reminder for every prayer

### `--adhan [enable <prayer>] [disable <prayer>] [set <path>]`

Control the adhan (call to prayer) audio that plays at the exact prayer time.

- `muslimtify notification --adhan enable fajr` — play the adhan at Fajr
- `muslimtify notification --adhan disable fajr` — don't play the adhan at Fajr
- `muslimtify notification --adhan set <path>` — use your own adhan file (mp3/wav/flac); leave it unset to use the bundled adhan

While the adhan plays it can be stopped from the notification (Stop button) or with `muslimtify sound stop`.

## `muslimtify method`

Set the calculation method used to compute prayer times. With no argument it shows the current method and madzhab.

- Usage: `muslimtify method <name>`, e.g. `muslimtify method kemenag`

| Key | Authority |
| --- | --- |
| `mwl` | Muslim World League |
| `makkah` | Umm al-Qura University, Makkah |
| `isna` | Islamic Society of North America |
| `egypt` | Egyptian General Authority of Survey |
| `karachi` | University of Islamic Sciences, Karachi |
| `kemenag` | Kementerian Agama, Indonesia (default) |

Run `muslimtify method list` to print them with your current choice marked.

## `muslimtify madzhab`

Set the madzhab (school of jurisprudence), which changes how the **Asr** time is calculated.

- Usage: `muslimtify madzhab <shafi|hanafi>`, e.g. `muslimtify madzhab hanafi`
- `shafi` — Asr starts when an object's shadow equals its length (default; also Maliki/Hanbali)
- `hanafi` — Asr starts when the shadow is twice the object's length, so Asr falls later

