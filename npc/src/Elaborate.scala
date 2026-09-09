object Elaborate {
  def main(args: Array[String]): Unit = {
    val emitSoC         = args.contains("--soc")
    val resetPcOption   = args.sliding(2).collectFirst { case Array("--reset-pc", value) =>
      if (value.startsWith("0x")) BigInt(value.drop(2), 16) else BigInt(value)
    }
    val stageArgs       = args.indices.collect {
      case index
          if args(index) != "--soc" && args(index) != "--reset-pc" &&
            (index == 0 || args(index - 1) != "--reset-pc") =>
        args(index)
    }.toArray
    val loweringOptions = "disallowLocalVariables,disallowPackedArrays,locationInfoStyle=wrapInAtSquareBracket"
    val firtoolOptions  = Array(
      "--default-layer-specialization=enable",
      "--verification-flavor=immediate",
      // Keep generated SystemVerilog compatible with the Yosys flow.
      "--lowering-options=" + loweringOptions
    )
    if (emitSoC) {
      val resetPc = resetPcOption.getOrElse(BigInt("20000000", 16))
      circt.stage.ChiselStage.emitSystemVerilogFile(new npc.ysyx_24100022(resetPc), stageArgs, firtoolOptions)
    } else {
      circt.stage.ChiselStage.emitSystemVerilogFile(new npc.NPC(), stageArgs, firtoolOptions)
    }
  }
}
