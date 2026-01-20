local dap = require("dap")

dap.configurations.cpp = {
  {
    name = "Debug (gdb)",
    type = "gdb",
    request = "launch",
    program = vim.fn.getcwd() .. "/extras/test/build/supladevicetests",
    cwd = vim.fn.getcwd(),
  },
}
dap.configurations.c = dap.configurations.cpp

