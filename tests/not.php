<?php

if (!true) {
    echo "NOT EXECUTED\n";
}

if (!false) {
    echo "EXECUTED\n";
}

$x = 0;

if (!!!$x) {
    echo "EXECUTED\n";
}
