<?php

$a = 0 + 0;
$a = 42;

$a = $a + 0;
$a = 0 + $a;
$a = 0 + $a + $a - $a + 0;

echo $a;
