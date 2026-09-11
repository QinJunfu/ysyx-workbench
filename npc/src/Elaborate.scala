object Elaborate {
  def main(args: Array[String]): Unit = {
    // The emitted top module is npc.ysyx_24100022 on every platform, so --soc only selects the ysyxSoC address-space
    // personality now; it no longer changes the module name.
    val narrowAddresses = args.contains("--narrow-addresses")
    val resetPcOption   = args.sliding(2).collectFirst { case Array("--reset-pc", value) =>
      if (value.startsWith("0x")) BigInt(value.drop(2), 16) else BigInt(value)
    }
    val switches        = Set("--soc", "--narrow-addresses", "--reset-pc")
    val stageArgs       = args.indices.collect {
      case index
          if !switches.contains(args(index)) &&
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
    // One top module name on every platform: the only difference between the Direct NPC and ysyxSoC builds is the
    // reset vector and whether the CPU is given a narrow physical address space.
    val resetPc         = resetPcOption.getOrElse(BigInt("80000000", 16))
    circt.stage.ChiselStage.emitSystemVerilogFile(
      new npc.ysyx_24100022(resetPc, narrowAddresses),
      stageArgs,
      firtoolOptions
    )
  }
}
