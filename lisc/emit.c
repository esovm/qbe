#include "lisc.h"
#include <stdarg.h>

static struct {
	int op;
	int cls;
	char *asm;
} omap[] = {
	{ OAdd,    -1, "=add%k %0, %1" },
	{ OSub,    -1, "=sub%k %0, %1" },
	{ OAnd,    -1, "=and%k %0, %1" },
	{ OMul,    -1, "=imul%k %0, %1" },
	{ OStorel, -1, "movq %L0, %M1" },
	{ OStorew, -1, "movl %W0, %M1" },
	{ OStoreh, -1, "movw %H0, %M1" },
	{ OStoreb, -1, "movb %B0, %M1" },
	{ OLoadl,  Kl, "movq %M0, %=" },
	{ OLoadsw, Kl, "movslq %M0, %L=" },
	{ OLoadsw, Kw, "movl %M0, %W=" },
	{ OLoaduw, -1, "movl %M0, %W=" },
	{ OLoadsh, -1, "movsw%k %M0, %=" },
	{ OLoaduh, -1, "movzw%k %M0, %=" },
	{ OLoadsb, -1, "movsb%k %M0, %=" },
	{ OLoadub, -1, "movzb%k %M0, %=" },
	{ OExtsw,  Kl, "movsq %W0, %L=" },
	{ OExtuw,  Kl, "movl %W0, %W=" },
	{ OExtsh,  -1, "movsw%k %H0, %=" },
	{ OExtuh,  -1, "movzw%k %H0, %=" },
	{ OExtsb,  -1, "movsb%k %B0, %=" },
	{ OExtub,  -1, "movzb%k %B0, %=" },
	{ OAddr,   -1, "lea%k %M0, %=" },
	{ OSwap,   -1, "xchg%k %0, %1" },
	{ OSign,   Kl, "cqto" },
	{ OSign,   Kw, "cltd" },
	{ OSAlloc, Kl, "subq %L0, %%rsp" }, /* fixme, ignores return value */
	{ OXPush,  -1, "push%k %0" },
	{ OXDiv,   -1, "idiv%k %0" },
	{ OXCmp,   -1, "cmp%k, %0, %1" },
	{ OXTest,  -1, "test%k %0, %1" },
	{ ONop,    -1, "" },
	{ OXSet+Ceq,  -1, "setz %B=\n\tmovzb%k %B=, %=" },
	{ OXSet+Csle, -1, "setle %B=\n\tmovzb%k %B=, %=" },
	{ OXSet+Cslt, -1, "setl %B=\n\tmovzb%k %B=, %=" },
	{ OXSet+Csgt, -1, "setg %B=\n\tmovzb%k %B=, %=" },
	{ OXSet+Csge, -1, "setge %B=\n\tmovzb%k %B=, %=" },
	{ OXSet+Cne,  -1, "setnz %B=\n\tmovzb%k %B=, %=" },
};

enum { SLong, SWord, SShort, SByte };
static char *rsub[][4] = {
	[RXX] = {"BLACK CAT", "BROKEN MIRROR", "666", "NOOOO!"},
	[RAX] = {"rax", "eax", "ax", "al"},
	[RBX] = {"rbx", "ebx", "bx", "bl"},
	[RCX] = {"rcx", "ecx", "cx", "cl"},
	[RDX] = {"rdx", "edx", "dx", "dl"},
	[RSI] = {"rsi", "esi", "si", "sil"},
	[RDI] = {"rdi", "edi", "di", "dil"},
	[RBP] = {"rbp", "ebp", "bp", "bpl"},
	[RSP] = {"rsp", "esp", "sp", "spl"},
	[R8 ] = {"r8" , "r8d", "r8w", "r8b"},
	[R9 ] = {"r9" , "r9d", "r9w", "r9b"},
	[R10] = {"r10", "r10d", "r10w", "r10b"},
	[R11] = {"r11", "r11d", "r11w", "r11b"},
	[R12] = {"r12", "r12d", "r12w", "r12b"},
	[R13] = {"r13", "r13d", "r13w", "r13b"},
	[R14] = {"r14", "r14d", "r14w", "r14b"},
	[R15] = {"r15", "r15d", "r15w", "r15b"},
};

static char *ctoa[NCmp] = {
	[Ceq ] = "e",
	[Csle] = "le",
	[Cslt] = "l",
	[Csgt] = "g",
	[Csge] = "ge",
	[Cne ] = "ne",
};

static int
slot(int s, Fn *fn)
{
	struct { int i:14; } x;

	x.i = s;
	assert(NAlign == 3);
	if (x.i < 0)
		return -4 * x.i;
	else {
		assert(fn->slot >= x.i);
		return -4 * (fn->slot - x.i);
	}
}

static void
emitcon(Con *con, FILE *f)
{
	switch (con->type) {
	default:
		diag("emit: invalid constant");
	case CAddr:
		fputs(con->label, f);
		if (con->val)
			fprintf(f, "%+"PRId64, con->val);
		break;
	case CNum:
		fprintf(f, "%"PRId64, con->val);
		break;
	}
}

static void
emitf(Fn *fn, FILE *f, char *fmt, ...)
{
	static char stoa[] = "qlwb";
	va_list ap;
	char c, *s, *s1;
	int i, ty;
	Ref ref;
	Mem *m;
	Con off;

	va_start(ap, fmt);
	ty = SWord;
	s = fmt;
	fputc('\t', f);
Next:
	while ((c = *s++) != '%')
		if (!c) {
			va_end(ap);
			fputc('\n', f);
			return;
		} else
			fputc(c, f);
	switch ((c = *s++)) {
	default:
		diag("emit: unknown escape");
	case 'w':
	case 'W':
		i = va_arg(ap, int);
		ty = i ? SLong: SWord;
	if (0) {
	case 't':
	case 'T':
		ty = va_arg(ap, int);
	}
		if (c == 't' || c == 'w')
			fputc(stoa[ty], f);
		break;
	case 's':
		s1 = va_arg(ap, char *);
		fputs(s1, f);
		break;
	case 'R':
		ref = va_arg(ap, Ref);
		switch (rtype(ref)) {
		default:
			diag("emit: invalid reference");
		case RTmp:
			assert(isreg(ref));
			fprintf(f, "%%%s", rsub[ref.val][ty]);
			break;
		case RSlot:
			fprintf(f, "%d(%%rbp)", slot(ref.val, fn));
			break;
		case RAMem:
		Mem:
			m = &fn->mem[ref.val & AMask];
			if (rtype(m->base) == RSlot) {
				off.type = CNum;
				off.val = slot(m->base.val, fn);
				addcon(&m->offset, &off);
				m->base = TMP(RBP);
			}
			if (m->offset.type != CUndef)
				emitcon(&m->offset, f);
			if (req(m->base, R) && req(m->index, R))
				break;
			fputc('(', f);
			if (!req(m->base, R))
				fprintf(f, "%%%s", rsub[m->base.val][SLong]);
			if (!req(m->index, R))
				fprintf(f, ", %%%s, %d",
					rsub[m->index.val][SLong],
					m->scale
				);
			fputc(')', f);
			break;
		case RCon:
			fputc('$', f);
			emitcon(&fn->con[ref.val], f);
			break;
		}
		break;
	case 'M':
		ref = va_arg(ap, Ref);
		switch (rtype(ref)) {
		default:
			diag("emit: invalid memory reference");
		case RAMem:
			goto Mem;
		case RSlot:
			fprintf(f, "%d(%%rbp)", slot(ref.val, fn));
			break;
		case RCon:
			emitcon(&fn->con[ref.val], f);
			break;
		case RTmp:
			assert(isreg(ref));
			fprintf(f, "(%%%s)", rsub[ref.val][SLong]);
			break;
		}
		break;
	}
	goto Next;
}

static int
cneg(int cmp)
{
	switch (cmp) {
	default:   diag("cneg: unhandled comparison");
	case Ceq:  return Cne;
	case Csle: return Csgt;
	case Cslt: return Csge;
	case Csgt: return Csle;
	case Csge: return Cslt;
	case Cne:  return Ceq;
	}
}

static void
eins(Ins i, Fn *fn, FILE *f)
{
	static char *otoa[NOp] = {
		[OAdd]    = "add",
		[OSub]    = "sub",
		[OMul]    = "imul",
		[OAnd]    = "and",
		[OLoad+Tl]  = "mov",
		[OLoad+Tsw] = "movsl",
		/* [OLoad+Tuw] treated manually */
		[OLoad+Tsh] = "movsw",
		[OLoad+Tuh] = "movzw",
		[OLoad+Tsb] = "movsb",
		[OLoad+Tub] = "movzb",
	};
	Ref r0, r1;
	int64_t val;

	switch (i.op) {
	case OMul:
		if (rtype(i.arg[1]) == RCon) {
			r0 = i.arg[1];
			r1 = i.arg[0];
		} else {
			r0 = i.arg[0];
			r1 = i.arg[1];
		}
		if (rtype(r0) == RCon && rtype(r1) == RTmp) {
			emitf(fn, f, "imul%w %R, %R, %R",
				i.wide, r0, r1, i.to);
			break;
		}
		/* fall through */
	case OAdd:
	case OSub:
	case OAnd:
		if (req(i.to, i.arg[1])) {
			if (i.op == OSub) {
				emitf(fn, f, "neg%w %R", i.wide, i.to);
				emitf(fn, f, "add%w %R, %R",
					i.wide, i.arg[0], i.to);
				break;
			}
			i.arg[1] = i.arg[0];
			i.arg[0] = i.to;
		}
		if (!req(i.to, i.arg[0]))
			emitf(fn, f, "mov%w %R, %R",
				i.wide, i.arg[0], i.to);
		emitf(fn, f, "%s%w %R, %R", otoa[i.op],
			i.wide, i.arg[1], i.to);
		break;
	case OCopy:
		if (req(i.to, R) || req(i.arg[0], R))
			break;
		if (isreg(i.to)
		&& i.wide
		&& rtype(i.arg[0]) == RCon
		&& fn->con[i.arg[0].val].type == CNum
		&& (val = fn->con[i.arg[0].val].val) >= 0
		&& val <= UINT32_MAX) {
			emitf(fn, f, "movl %R, %R", i.arg[0], i.to);
		} else if (!req(i.arg[0], i.to))
			emitf(fn, f, "mov%w %R, %R",
				i.wide, i.arg[0], i.to);
		break;
	case OStorel:
	case OStorew:
	case OStoreh:
	case OStoreb:
		emitf(fn, f, "mov%t %R, %M",
			i.op - OStorel, i.arg[0], i.arg[1]);
		break;
	case OLoad+Tuw:
		emitf(fn, f, "movl %M, %R", i.arg[0], i.to);
		break;
	case OLoad+Tsw:
		if (i.wide == 0) {
			emitf(fn, f, "movl %M, %R", i.arg[0], i.to);
			break;
		}
	case OLoad+Tl:
	case OLoad+Tsh:
	case OLoad+Tuh:
	case OLoad+Tsb:
	case OLoad+Tub:
		emitf(fn, f, "%s%w %M, %R", otoa[i.op],
			i.wide, i.arg[0], i.to);
		break;
	case OExt+Tuw:
		emitf(fn, f, "movl %R, %R", i.arg[0], i.to);
		break;
	case OExt+Tsw:
	case OExt+Tsh:
	case OExt+Tuh:
	case OExt+Tsb:
	case OExt+Tub:
		emitf(fn, f, "mov%s%t%s %R, %W%R",
			(i.op-OExt-Tsw)%2 ? "z" : "s",
			1+(i.op-OExt-Tsw)/2,
			i.wide ? "q" : "l",
			i.arg[0], i.wide, i.to);
		break;
	case OCall:
		switch (rtype(i.arg[0])) {
		default:
			diag("emit: invalid call instruction");
		case RCon:
			emitf(fn, f, "callq %M", i.arg[0]);
			break;
		case RTmp:
			emitf(fn, f, "call%w *%R", 1, i.arg[0]);
			break;
		}
		break;
	case OAddr:
		emitf(fn, f, "lea%w %M, %R", i.wide, i.arg[0], i.to);
		break;
	case OSwap:
		emitf(fn, f, "xchg%w %R, %R", i.wide, i.arg[0], i.arg[1]);
		break;
	case OSign:
		if (req(i.to, TMP(RDX)) && req(i.arg[0], TMP(RAX))) {
			if (i.wide)
				fprintf(f, "\tcqto\n");
			else
				fprintf(f, "\tcltd\n");
		} else
			diag("emit: unhandled instruction (2)");
		break;
	case OSAlloc:
		emitf(fn, f, "sub%w %R, %R", 1, i.arg[0], TMP(RSP));
		if (!req(i.to, R))
			emitf(fn, f, "mov%w %R, %R", 1, TMP(RSP), i.to);
		break;
	case OXPush:
		emitf(fn, f, "push%w %R", i.wide, i.arg[0]);
		break;
	case OXDiv:
		emitf(fn, f, "idiv%w %R", i.wide, i.arg[0]);
		break;
	case OXCmp:
		emitf(fn, f, "cmp%w %R, %R", i.wide, i.arg[0], i.arg[1]);
		break;
	case OXTest:
		emitf(fn, f, "test%w %R, %R", i.wide, i.arg[0], i.arg[1]);
		break;
	case ONop:
		break;
	default:
		if (OXSet <= i.op && i.op <= OXSet1) {
			emitf(fn, f, "set%s%t %R",
				ctoa[i.op-OXSet], SByte, i.to);
			emitf(fn, f, "movzb%w %T%R, %W%R",
				i.wide, SByte, i.to, i.wide, i.to);
			break;
		}
		diag("emit: unhandled instruction (3)");
	}
}

static int
framesz(Fn *fn)
{
	int i, o, f;

	assert(NAlign == 3);
	for (i=0, o=0; i<NRClob; i++)
		o ^= 1 & (fn->reg >> rclob[i]);
	f = fn->slot;
	f = (f + 3) & -4;
	return 4*f + 8*o;
}

void
emitfn(Fn *fn, FILE *f)
{
	Blk *b, *s;
	Ins *i;
	int *r, c, fs;

	fprintf(f,
		".text\n"
		".globl %s\n"
		".type %s, @function\n"
		"%s:\n"
		"\tpush %%rbp\n"
		"\tmov %%rsp, %%rbp\n",
		fn->name, fn->name, fn->name
	);
	fs = framesz(fn);
	if (fs)
		fprintf(f, "\tsub $%d, %%rsp\n", fs);
	for (r=rclob; r-rclob < NRClob; r++)
		if (fn->reg & BIT(*r))
			emitf(fn, f, "push%w %R", 1, TMP(*r));

	for (b=fn->start; b; b=b->link) {
		fprintf(f, ".L%s:\n", b->name);
		for (i=b->ins; i-b->ins < b->nins; i++)
			eins(*i, fn, f);
		switch (b->jmp.type) {
		case JRet0:
			for (r=&rclob[NRClob]; r>rclob;)
				if (fn->reg & BIT(*--r))
					emitf(fn, f, "pop%w %R", 1, TMP(*r));
			fprintf(f,
				"\tleave\n"
				"\tret\n"
			);
			break;
		case JJmp:
			if (b->s1 != b->link)
				fprintf(f, "\tjmp .L%s\n", b->s1->name);
			break;
		default:
			c = b->jmp.type - JXJc;
			if (0 <= c && c <= NCmp) {
				if (b->link == b->s2) {
					s = b->s1;
				} else if (b->link == b->s1) {
					c = cneg(c);
					s = b->s2;
				} else
					diag("emit: unhandled jump (1)");
				fprintf(f, "\tj%s .L%s\n", ctoa[c], s->name);
				break;
			}
			diag("emit: unhandled jump (2)");
		}
	}

}

void
emitdat(Dat *d, FILE *f)
{
	static char *dtoa[] = {
		[DAlign] = ".align",
		[DB] = "\t.byte",
		[DH] = "\t.value",
		[DW] = "\t.long",
		[DL] = "\t.quad"
	};

	switch (d->type) {
	case DStart:
		fprintf(f, ".data\n");
		break;
	case DEnd:
		break;
	case DName:
		fprintf(f,
			".globl %s\n"
			".type %s, @object\n"
			"%s:\n",
			d->u.str, d->u.str, d->u.str
		);
		break;
	case DA:
		fprintf(f, "\t.string \"%s\"\n", d->u.str);
		break;
	default:
		fprintf(f, "%s %"PRId64"\n", dtoa[d->type], d->u.num);
		break;
	}
}
