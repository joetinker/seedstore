## EEPROM Loader - driver for seedstore

# echo "Serial ports:"
$ports = [System.IO.Ports.SerialPort]::getportnames()
# echo ""

echo "List of available COM ports:";
$pnum = 1;
foreach ($PortName in $ports) {
    "$PortName - $pnum"; $pnum++
}

do {
  write-host -nonewline "Enter the COM port number:"
  $inputString = read-host
  $value = $inputString -as [Int16]
  $ok = $value -ne $NULL
  if ( -not $ok ) { write-host "You must enter a numeric value" }
}
until ( $ok )

$i = 1
foreach ($PortName in $ports) {
  $selectedPort = $PortName;
  if ($i -eq $value) { break }
  $i++; 
}

write-host "You selected: $selectedPort, waiting for device."; 

#$words = Get-Content "J:\Projekty HW\Arduino\SEEDSTORE\EEPROM\wordlist.txt"
$words = Get-Content ".\wordlist.txt"
$port = new-Object System.IO.Ports.SerialPort $selectedPort,9600,None,8,one

$port.open()
Start-Sleep -milliseconds 100
$port.ReadLine()

$port.WriteLine(“BEGIN”)
Write-Host "BEGIN:" -NoNewline; $port.ReadLine()

foreach ($line in $words) {
    $port.WriteLine($line)
    #Write-Host "$line " -NoNewline; 
    $port.ReadLine()
    #Start-Sleep -milliseconds 5
}

$fail = 0;
$port.WriteLine(“VERIFY”)
Write-Host "VERIFY:" -NoNewline; $port.ReadLine()

$lineNum = 0
foreach ($line in $words) {
    $port.WriteLine($line)
    # Write-Host "$line " -NoNewline; 
    $x = $port.ReadLine()
    
    if ($x -match "OK") {Write-Host "*" -NoNewline } else {$fail = 1; break}

    if ($lineNum % 80 -eq 79) {Write-Host "" }

    $lineNum++
}
#$port.WriteLine(“END”)
#echo "END"

$port.Close()
if ($fail -eq 1) {Write-Host "Loading failed!"}
Else {Write-Host "Loading succeeded"}
