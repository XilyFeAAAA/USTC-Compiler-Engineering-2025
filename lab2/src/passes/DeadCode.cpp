#include "DeadCode.hpp"
#include "Instruction.hpp"
#include "logging.hpp"
#include <stdexcept>
#include <memory>
#include <vector>


void DeadCode::run() {
    bool changed{};
    func_info->run();
    do {
        changed = false;
        for (auto &F : m_->get_functions()) {
            auto func = &F;
            changed |= clear_basic_blocks(func);
            mark(func);
            changed |= sweep(func);
        }
    } while (changed);
    LOG_INFO << "dead code pass erased " << ins_count << " instructions";
}

bool DeadCode::clear_basic_blocks(Function *func) {
    bool changed = 0;
    std::vector<BasicBlock *> to_erase;
    for (auto &bb1 : func->get_basic_blocks()) {
        auto bb = &bb1;
        if(bb->get_pre_basic_blocks().empty() && bb != func->get_entry_block()) {
            to_erase.push_back(bb);
            changed = 1;
        }
    }
    for (auto &bb : to_erase) {
        bb->erase_from_parent();
        delete bb;
    }
    return changed;
}

void DeadCode::mark(Function *func) {
    if (!func) return;
    for (auto &bb : func->get_basic_blocks()) {
        for (auto &bi : bb.get_instructions()) {
            Instruction *i = &bi;
            if (is_critical(i)) {
                mark(i);
            }
        }
    }
}

void DeadCode::mark(Instruction *ins) {
    if (!ins) return;
    if (marked.find(ins) == marked.end()){
        
        marked[ins] = true;
        for (auto op : ins->get_operands()) {
            if (!op) continue;
            if (auto ins = dynamic_cast<Instruction *>(op)) mark(ins);
        }

    }
    
}

bool DeadCode::sweep(Function *func) {
    
    for (auto &bb : func->get_basic_blocks()) {
        std::vector<Instruction *> to_remove;

        for (auto &bi : bb.get_instructions()) {
            Instruction *ins = &bi;
            if (marked.find(ins) == marked.end() || !marked[ins]) {
                to_remove.push_back(ins);
            }
        }

        if (to_remove.empty()) return false;

        for (Instruction* ins : to_remove) {
            if (!ins) continue;
            auto users = ins->get_use_list();
            for (auto &u : users) {
                User *user = u.val_;
                if (!user) continue;
                if (auto ui = dynamic_cast<Instruction *>(user)) {
                    ui->remove_operand(u.arg_no_);
                }
            }
            ins->remove_all_operands();
            bb.remove_instr(ins);
            ins_count++;
            delete ins;
        }
    }
    
    return true;
}

bool DeadCode::is_critical(Instruction *ins) {
    if (!ins) return false;
    if (!ins->get_use_list().empty()) return true;

    if (ins->is_call()) {
        auto ci = static_cast<CallInst *>(ins);
        auto cf = ci->func_;
        bool pure = false;
        try {
            pure = func_info->is_pure_function(cf);
        } catch (const std::out_of_range &) {
            pure = false;
        }
        return !pure;
    }

    if (ins->is_br()) return true;

    if (ins->is_ret()) return true;

    if (ins->is_store()) return true;

    return false;
}

void DeadCode::sweep_globally() {
    std::vector<Function *> unused_funcs;
    std::vector<GlobalVariable *> unused_globals;
    for (auto &f_r : m_->get_functions()) {
        if (f_r.get_use_list().size() == 0 and f_r.get_name() != "main")
            unused_funcs.push_back(&f_r);
    }
    for (auto &glob_var_r : m_->get_global_variable()) {
        if (glob_var_r.get_use_list().size() == 0)
            unused_globals.push_back(&glob_var_r);
    }
    // changed |= unused_funcs.size() or unused_globals.size();
    for (auto func : unused_funcs)
        m_->get_functions().erase(func);
    for (auto glob : unused_globals)
        m_->get_global_variable().erase(glob);
}
