from m5.proxy import Self

from m5.objects.BaseRunaheadCPU import BaseRunaheadCPU
from m5.objects.X86MMU import X86MMU
from m5.objects.X86CPU import X86CPU

class X86RunaheadCPU(BaseRunaheadCPU, X86CPU):
    mmu = X86MMU()
    needsTSO = True
    numPhysCCRegs = Self.numPhysIntRegs * 5
