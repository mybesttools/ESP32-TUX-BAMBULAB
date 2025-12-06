#!/bin/bash
# Extract Bambu Lab printer certificate
# Usage: ./get_bambu_cert.sh <printer_ip>

PRINTER_IP=${1:-10.13.13.85}
PORT=8883

echo "Connecting to Bambu printer at $PRINTER_IP:$PORT to extract certificate..."
echo "Press Ctrl+C after connection is established"

# Extract the certificate
openssl s_client -showcerts -connect $PRINTER_IP:$PORT </dev/null 2>/dev/null | \
openssl x509 -outform PEM > bambu_printer.pem

if [ -s bambu_printer.pem ]; then
    echo "Certificate saved to bambu_printer.pem"
    echo ""
    echo "Certificate details:"
    openssl x509 -in bambu_printer.pem -text -noout | grep -A2 "Subject:"
    echo ""
    echo "Add this to your code:"
    echo 'static const char bambu_cert_pem[] = "'
    cat bambu_printer.pem | tr '\n' '|' | sed 's/|/\\n"\n"/g'
    echo '";'
else
    echo "Failed to extract certificate. Make sure printer is accessible at $PRINTER_IP:$PORT"
    exit 1
fi
