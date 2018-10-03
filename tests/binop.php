<?php

if (true && false)
    echo "NOT EXECUTED\n";
if (true || false)
    echo "EXECUTED true || false \n";

if (true && true)
    echo "EXECUTED true && true\n";

if (false && false)
    echo "NOT EXECUTED\n";

$var = true || false;

if ($var) {
    echo "EXECUTED var\n";
}

if ("Hallo" == "Hallo") {
    echo "EXECUTED Hallo==Hallo\n";
}

if ("Hallo" == "hallo") 
    echo "NOT EXECUTED\n";

if (1 == 2) 
    echo "NOT EXECUTED \n";

if (42 == 21*2) {
  echo "EXECUTED 42 == 21*2\n";
}

if (1 < 2) {
  echo "EXECUTED 1 < 2\n";
}

if (1 > 2) {
  echo "NOT EXECUTED\n";
}


if (1 <= 2) {
  echo "EXECUTED 1 <= 2\n";
}

if (1 >= 2) {
  echo "NOT EXECUTED\n";
}
