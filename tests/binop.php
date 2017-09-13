<?php

if (true && false)
    echo "NOT EXECUTED\n";
if (true || false)
    echo "EXECUTED\n";

if (true && true)
    echo "EXECUTED\n";

if (false && false)
    echo "NOT EXECUTED\n";

$var = true || false;

if ($var) {
    echo "EXECUTED\n";
}

if ("Hallo" == "Hallo") {
    echo "EXECUTED\n";
}

if ("Hallo" == "hallo") 
    echo "NOT EXECUTED\n";

if (1 == 2) 
    echo "NOT EXECUTED\n";

if (42 == 21*2) {
  echo "EXECUTED\n";
}
