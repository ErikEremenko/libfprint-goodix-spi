import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class DecompileFunctions extends GhidraScript {
    @Override
    protected void run() throws Exception {
        DecompInterface decompiler = new DecompInterface();

        decompiler.toggleCCode(true);
        decompiler.toggleSyntaxTree(true);
        if (!decompiler.openProgram(currentProgram)) {
            printerr("could not initialize decompiler");
            return;
        }

        for (String argument : getScriptArgs()) {
            Address address = toAddr(argument);
            Function function = getFunctionAt(address);

            if (function == null)
                function = getFunctionContaining(address);
            if (function == null) {
                printerr("no function at " + argument);
                continue;
            }

            println("===== " + function.getName() + " @ " +
                    function.getEntryPoint() + " =====");
            DecompileResults result =
                decompiler.decompileFunction(function, 180, monitor);
            if (!result.decompileCompleted()) {
                printerr("decompile failed: " + result.getErrorMessage());
                continue;
            }
            println(result.getDecompiledFunction().getC());
        }

        decompiler.dispose();
    }
}
