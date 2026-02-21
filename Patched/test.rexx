/* test.rexx */
Options FailAt 100

Options Results

IF ~SHOW('P','SHITTYPLAYER') THEN DO
    SAY "SHITTYPLAYER not running."
    EXIT
END

ADDRESS SHITTYPLAYER

/* get the title */
'GETTITLE'
SAY 'Current song:' RESULT

/* get the artist */
'GETARTIST'
SAY 'Current artist:' RESULT

/* get the played time */
'GETLAPTIME'
total_seconds = RESULT+0 /* Beispielwert */

hours   = total_seconds % 3600
rest    = total_seconds // 3600
minutes = rest % 60
seconds = rest // 60
time_string = right(hours, 2, '0')":"right(minutes, 2, '0')":"right(seconds, 2, '0')

SAY 'Lap time:' time_string

/* get the song duration */
'GETDURATION'
total_seconds = RESULT+0 /* Beispielwert */

hours   = total_seconds % 3600
rest    = total_seconds // 3600
minutes = rest % 60
seconds = rest // 60

/* Mit RIGHT formatieren, damit führende Nullen entstehen */
time_string = right(hours, 2, '0')":"right(minutes, 2, '0')":"right(seconds, 2, '0')
SAY 'Durations (seconds):' time_string

EXIT