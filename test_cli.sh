LSPX_TRACE_RPC=1 LSPX_TRACE_LSP=1 LSPX_TRACE_GRAPH=1 \
./build/lspx/cli/lspx-cli \
  jdtls callgraph \
  --java java \
  --jdtls-home /home/tszyc/llp/etc/jdtls/1.57.0 \
  --root /home/tszyc/llp/etc/mini-java-playground \
  --workspace /tmp/lspx-jdtls-workspace \
  --file /home/tszyc/llp/etc/mini-java-playground/src/main/java/com/acme/playground/CheckoutService.java \
  --function finalizeCheckout \
  --max-depth 5 \
  --direction both
