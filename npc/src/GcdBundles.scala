package gcd

import chisel3._

/** Input values for the streaming GCD example. */
class GcdInputBundle(val w: Int) extends Bundle {
  val value1 = UInt(w.W)
  val value2 = UInt(w.W)
}

/** Result values for the streaming GCD example. */
class GcdOutputBundle(val w: Int) extends Bundle {
  val value1 = UInt(w.W)
  val value2 = UInt(w.W)
  val gcd    = UInt(w.W)
}

/** Explicit producer-to-GCD handshake port. */
class GcdInputPort(width: Int) extends Bundle {
  val valid = Input(Bool())
  val ready = Output(Bool())
  val bits  = Input(new GcdInputBundle(width))
}

/** Explicit GCD-to-consumer handshake port. */
class GcdOutputPort(width: Int) extends Bundle {
  val valid = Output(Bool())
  val ready = Input(Bool())
  val bits  = Output(new GcdOutputBundle(width))
}
