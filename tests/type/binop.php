<?php

echo gettype("" || "Hallo") . "\n";
echo gettype(0 && "Hallo") . "\n";
echo gettype(false && true) . "\n";
echo gettype(1 == 3) . "\n";
