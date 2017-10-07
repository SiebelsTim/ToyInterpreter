<?php

function fun($num) {
    echo "Counter: " . $num . "\n";
}

for ($i = 0; $i < 7; ++$i) {
    fun($i);
    echo idxToDay($i) . "\n";
}


function idxToDay($num) {
    if ($num == 0) {
        return "Monday";
    }
    if ($num == 1) {
        return "Tuesday";
    }
    if ($num == 2) {
        return "Wednesday";
    }
    if ($num == 3) {
        return "Thursday";
    }
    if ($num == 4) {
        return "Friday";
    }
    if ($num == 5) {
        return "Saturday";
    }
    if ($num == 6) {
        return "Sunday";
    }

    return "Unknown Day";
}