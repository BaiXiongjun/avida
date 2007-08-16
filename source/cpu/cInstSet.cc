/*
 *  cInstSet.cc
 *  Avida
 *
 *  Called "inst_set.cc" prior to 12/5/05.
 *  Copyright 1999-2007 Michigan State University. All rights reserved.
 *  Copyright 1993-2001 California Institute of Technology.
 *
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; version 2
 *  of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include "cInstSet.h"

#include "cArgContainer.h"
#include "cArgSchema.h"
#include "cAvidaContext.h"
#include "cInitFile.h"
#include "cStringUtil.h"
#include "cWorld.h"
#include "cWorldDriver.h"

using namespace std;


bool cInstSet::OK() const
{
  assert(m_lib_name_map.GetSize() < 256);
  assert(m_lib_nopmod_map.GetSize() < m_lib_name_map.GetSize());

  // Make sure that all of the redundancies are represented the appropriate
  // number of times.
  tArray<int> test_redundancy2(m_lib_name_map.GetSize());
  test_redundancy2.SetAll(0);
  for (int i = 0; i < m_mutation_chart.GetSize(); i++) {
    int test_id = m_mutation_chart[i];
    test_redundancy2[test_id]++;
  }
  for (int i = 0; i < m_lib_name_map.GetSize(); i++) {
    assert(m_lib_name_map[i].redundancy == test_redundancy2[i]);
  }

  return true;
}

cInstruction cInstSet::GetRandomInst(cAvidaContext& ctx) const
{
  int inst_op = m_mutation_chart[ctx.GetRandom().GetUInt(m_mutation_chart.GetSize())];
  return cInstruction(inst_op);
}


int cInstSet::AddInst(int lib_fun_id, int redundancy, int ft_cost, int cost, int energy_cost, double prob_fail, int addl_time_cost)
{
  if (lib_fun_id == m_inst_lib->GetInstNull().GetOp())
    m_world->GetDriver().RaiseFatalException(1,"Invalid use of NULL instruction");
  
  const int inst_id = m_lib_name_map.GetSize();

  assert(inst_id < 255);

  // Increase the size of the array...
  m_lib_name_map.Resize(inst_id + 1);

  // Setup the new function...
  m_lib_name_map[inst_id].lib_fun_id = lib_fun_id;
  m_lib_name_map[inst_id].redundancy = redundancy;
  m_lib_name_map[inst_id].cost = cost;
  m_lib_name_map[inst_id].ft_cost = ft_cost;
  m_lib_name_map[inst_id].energy_cost = energy_cost;
  m_lib_name_map[inst_id].prob_fail = prob_fail;
  m_lib_name_map[inst_id].addl_time_cost = addl_time_cost;

  const int total_redundancy = m_mutation_chart.GetSize();
  m_mutation_chart.Resize(total_redundancy + redundancy);
  for (int i = 0; i < redundancy; i++) {
    m_mutation_chart[total_redundancy + i] = inst_id;
  }
  total_energy_cost += energy_cost;

  return inst_id;
}


int cInstSet::AddNop(int lib_nopmod_id, int redundancy, int ft_cost, int cost, int energy_cost, double prob_fail, int addl_time_cost)
{
  // Assert nops are at the _beginning_ of an inst_set.
  assert(m_lib_name_map.GetSize() == m_lib_nopmod_map.GetSize());

  const int inst_id = AddInst(lib_nopmod_id, redundancy, ft_cost, cost, energy_cost, prob_fail, addl_time_cost);

  m_lib_nopmod_map.Resize(inst_id + 1);
  m_lib_nopmod_map[inst_id] = lib_nopmod_id;

  return inst_id;
}

cInstruction cInstSet::ActivateNullInst()
{  
  const int inst_id = m_lib_name_map.GetSize();
  const int null_fun_id = m_inst_lib->GetInstNull().GetOp();
  
  assert(inst_id < 255);
  
  // Make sure not to activate again if NULL is already active
  for (int i = 0; i < inst_id; i++) if (m_lib_name_map[i].lib_fun_id == null_fun_id) return cInstruction(i);
  
  
  // Increase the size of the array...
  m_lib_name_map.Resize(inst_id + 1);
  
  // Setup the new function...
  m_lib_name_map[inst_id].lib_fun_id = null_fun_id;
  m_lib_name_map[inst_id].redundancy = 0;
  m_lib_name_map[inst_id].cost = 0;
  m_lib_name_map[inst_id].ft_cost = 0;
  m_lib_name_map[inst_id].energy_cost = 0;
  m_lib_name_map[inst_id].prob_fail = 0.0;
  m_lib_name_map[inst_id].addl_time_cost = 0;
  
  return cInstruction(inst_id);
}


cString cInstSet::FindBestMatch(const cString& in_name) const
{
  int best_dist = 1024;
  cString best_name("");
  
  for (int i = 0; i < m_lib_name_map.GetSize(); i++) {
    const cString & cur_name = m_inst_lib->GetName(m_lib_name_map[i].lib_fun_id);
    const int cur_dist = cStringUtil::EditDistance(cur_name, in_name);
    if (cur_dist < best_dist) {
      best_dist = cur_dist;
      best_name = cur_name;
    }
    if (cur_dist == 0) break;
  }

  return best_name;
}

bool cInstSet::InstInSet(const cString& in_name) const
{
  cString best_name("");
  
  for (int i = 0; i < m_lib_name_map.GetSize(); i++) {
    const cString & cur_name = m_inst_lib->GetName(m_lib_name_map[i].lib_fun_id);
    if (cur_name == in_name) return true;
  }
  return false;
}

void cInstSet::LoadFromFile(const cString& filename)
{
  cArgSchema schema;
  
  // Integer
  schema.AddEntry("redundancy", 0, 1);
  schema.AddEntry("cost", 1, 0);
  schema.AddEntry("initial_cost", 2, 0);
  schema.AddEntry("energy_cost", 3, 0);
  schema.AddEntry("addl_time_cost", 4, 0);
  
  // Double
  schema.AddEntry("prob_fail", 0, 0.0);
  
  
  cInitFile file(filename);
  if (!file.WasOpened()) {
    m_world->GetDriver().RaiseFatalException(1, cString("Unable to load instruction set '") + filename + "'.");
  }
  
  tList<cString> errors;
  bool success = true;
  for (int line_id = 0; line_id < file.GetNumLines(); line_id++) {
    cString cur_line = file.GetLine(line_id);
    
    cString inst_name = cur_line.PopWord();
    int inst_idx = m_inst_lib->GetIndex(inst_name);
    if (inst_idx == -1) {
      // Oh oh!  Didn't find an instruction!
      cString* errorstr = new cString("Unknown instruction '");
      *errorstr += inst_name + "' (Best match = '" + m_inst_lib->GetNearMatch(inst_name) + "').";
      errors.PushRear(errorstr);
      success = false;
      continue;
    }
    
    cArgContainer* args = cArgContainer::Load(cur_line, schema, &errors);
    if (!args) {
      success = false;
      continue;
    }
    
    int redundancy = args->GetInt(0);
    int cost = args->GetInt(1);
    int ft_cost = args->GetInt(2);
    int energy_cost = args->GetInt(3);
    double prob_fail = args->GetDouble(0);
    int addl_time_cost = args->GetInt(4);
    
    delete args;
    
    if ((*m_inst_lib)[inst_idx].IsNop()) {
      AddNop(inst_idx, redundancy, ft_cost, cost, energy_cost, prob_fail, addl_time_cost);
    } else {
      AddInst(inst_idx, redundancy, ft_cost, cost, energy_cost, prob_fail, addl_time_cost);
    }
  }
  
  if (!success) {
    cString* errstr = NULL;
    while ((errstr = errors.Pop())) {
      m_world->GetDriver().RaiseException(*errstr);
      delete errstr;
    }
    m_world->GetDriver().RaiseFatalException(1,"Failed to load instruction set due to previous errors.");
  }
}


void cInstSet::LoadFromLegacyFile(const cString& filename)
{
  cInitFile file(filename);
  
  if (file.WasOpened() == false) {
    m_world->GetDriver().RaiseFatalException(1, cString("Could not open instruction set '") + filename + "'.");
  }
  
  for (int line_id = 0; line_id < file.GetNumLines(); line_id++) {
    cString cur_line = file.GetLine(line_id);
    cString inst_name = cur_line.PopWord();
    int redundancy = cur_line.PopWord().AsInt();
    int cost = cur_line.PopWord().AsInt();
    int ft_cost = cur_line.PopWord().AsInt();
    int energy_cost = cur_line.PopWord().AsInt();
    double prob_fail = cur_line.PopWord().AsDouble();
    int addl_time_cost = cur_line.PopWord().AsInt();
    
    // If this instruction has 0 redundancy, we don't want it!
    if (redundancy < 0) continue;
    if (redundancy > 256) {
      cString msg("Max redundancy is 256.  Resetting redundancy of \"");
      msg += inst_name; msg += "\" from "; msg += redundancy; msg += " to 256.";
      m_world->GetDriver().NotifyWarning(msg);
      redundancy = 256;
    }
    
    // Otherwise, this instruction will be in the set.
    // First, determine if it is a nop...
    int inst_idx = m_inst_lib->GetIndex(inst_name);
    
    if (inst_idx == -1) {
      // Oh oh!  Didn't find an instruction!
      cString errorstr("Could not find instruction '");
      errorstr += inst_name + "'\n        (Best match = '" + m_inst_lib->GetNearMatch(inst_name) + "').";
      m_world->GetDriver().RaiseFatalException(1, errorstr);
    }
    
    if ((*m_inst_lib)[inst_idx].IsNop()) {
      AddNop(inst_idx, redundancy, ft_cost, cost, energy_cost, prob_fail, addl_time_cost);
    } else {
      AddInst(inst_idx, redundancy, ft_cost, cost, energy_cost, prob_fail, addl_time_cost);
    }
  }
  
}
