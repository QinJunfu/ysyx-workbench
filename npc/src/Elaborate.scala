object Elaborate {
  def main(args: Array[String]): Unit = {
    val loweringOptions = "disallowLocalVariables,disallowPackedArrays,locationInfoStyle=wrapInAtSquareBracket"
    val firtoolOptions  = Array(
      "--default-layer-specialization=enable",
      "--verification-flavor=immediate",
      // Keep generated SystemVerilog compatible with the Yosys flow.
      "--lowering-options=" + loweringOptions
    )
    circt.stage.ChiselStage.emitSystemVerilogFile(new npc.NPC(), args, firtoolOptions)
  }
}
