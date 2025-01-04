/*
 * Copyright (c) 2020, 2024, Huawei Technologies Co., Ltd. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#ifndef SHARE_JBOLT_JBOLTCALLGRAPH_HPP
#define SHARE_JBOLT_JBOLTCALLGRAPH_HPP

#include "jbolt/jbolt_globals.hpp"
#include "jbolt/jBoltManager.hpp"
#include "jfr/utilities/jfrTypes.hpp"
#include "utilities/growableArray.hpp"

class JBoltFunc;
class JBoltCall;
class JBoltCluster;

template<typename T>
static GrowableArray<T>* create_growable_array(int size = 1) {
  GrowableArray<T>* array = new (ResourceObj::C_HEAP, mtTracing) GrowableArray<T>(size, mtTracing);
  assert(array != NULL, "invariant");
  return array;
}

// initial cluster id
static u4 _init_cluster_id = 0;

class JBoltCallGraph : public CHeapObj<mtTracing> {
  private:
    GrowableArray<JBoltCluster>* _clusters = NULL;
    GrowableArray<JBoltCall>* _calls = NULL;
    GrowableArray<JBoltFunc>* _funcs = NULL;

    JBoltCallGraph() {
        _clusters = create_growable_array<JBoltCluster>();
        _calls = create_growable_array<JBoltCall>();
        _funcs = create_growable_array<JBoltFunc>();
    }

    JBoltCallGraph(const JBoltCallGraph &) = delete;
    JBoltCallGraph(const JBoltCallGraph &&) = delete;

    // for constructing CG
    void add_func(JBoltFunc* func);     // Node
    void add_call(JBoltCall* call);     // Edge

  public:
    static JBoltCallGraph& callgraph_instance();
    // these two funcs initialize and deinitialize homonymous static array pointers in global
    static void initialize();
    static void deinitialize();

    GrowableArray<JBoltCluster>* callgraph_clusters()   { return _clusters; }
    GrowableArray<JBoltCall>* callgraph_calls()         { return _calls;    }
    GrowableArray<JBoltFunc>* callgraph_funcs()         { return _funcs;    }

    static void static_add_func(JBoltFunc* func) {  callgraph_instance().add_func(func);  }
    static void static_add_call(JBoltCall* call) {  callgraph_instance().add_call(call);  }

    // for dealing with CG
    GrowableArray<JBoltFunc>* hfsort();

    int clear_instance();

    virtual ~JBoltCallGraph() {
        delete _clusters;
        delete _calls;
        delete _funcs;

        _clusters = NULL;
        _calls = NULL;
        _funcs = NULL;
    }
};

class JBoltFunc : public CHeapObj<mtTracing> {
  private:
    const InstanceKlass* _klass;
    traceid _method_id;
    int64_t _heat;
    int _size;
    int _cluster_id;
    JBoltMethodKey _method_key;
    GrowableArray<int>* _call_indexes;

  public:
    JBoltFunc();
    JBoltFunc(const JBoltFunc& func);
    JBoltFunc(const InstanceKlass* klass, traceid method_id, int size, JBoltMethodKey method_key);

    virtual ~JBoltFunc() {
      delete _call_indexes;
    }

    bool operator==(const JBoltFunc& func) const    { return (_klass == func._klass && _method_id == func._method_id) || (_method_key.equals(func._method_key));      }
    bool operator!=(const JBoltFunc& func) const    { return (_klass != func._klass || _method_id != func._method_id) && !(_method_key.equals(func._method_key));     }

    JBoltFunc& operator=(const JBoltFunc& func) {
        _klass = func._klass;
        _method_id = func._method_id;
        _heat = func._heat;
        _size = func._size;
        _cluster_id = func._cluster_id;
        _method_key = func._method_key;
        if (_call_indexes != nullptr) {
            delete _call_indexes;
        }
        _call_indexes = create_growable_array<int>(func.get_calls_count());
        _call_indexes->appendAll(func.call_indexes());

        return *this;
    }

    const InstanceKlass* klass() const             { return _klass;                                                     }
    const traceid method_id() const                { return _method_id;                                                 }
    const int64_t heat() const                     { return _heat;                                                      }
    const int size() const                         { return _size;                                                      }
    const int cluster_id() const                   { return _cluster_id;                                                }
    JBoltMethodKey method_key() const              { return _method_key;                                                }
    GrowableArray<int>* call_indexes() const       { return _call_indexes;                                              }
    int get_calls_count() const                    { return _call_indexes->length();                                    }

    void add_heat(int64_t heat);
    void set_heat(int64_t heat);
    void set_cluster_id(int cluster_id);
    void append_call_index(int index);

    static ByteSize klass_offset()                 { return byte_offset_of(JBoltFunc, _klass);                          }
    static ByteSize method_id_offset()             { return byte_offset_of(JBoltFunc, _method_id);                      }
    static ByteSize heat_offset()                  { return byte_offset_of(JBoltFunc, _heat);                           }
    static ByteSize size_offset()                  { return byte_offset_of(JBoltFunc, _size);                           }
    static ByteSize cluster_id_offset()            { return byte_offset_of(JBoltFunc, _cluster_id);                     }
    static ByteSize call_indexes_offset()          { return byte_offset_of(JBoltFunc, _call_indexes);                   }

    static JBoltFunc* constructor(const InstanceKlass* klass, traceid method_id, int size, JBoltMethodKey method_key);
    static JBoltFunc* copy_constructor(const JBoltFunc* func);
};

class JBoltCluster : public CHeapObj<mtTracing> {
  private:
    int _id;
    int64_t _heats;
    bool _frozen;
    int _size;
    double _density;
    GrowableArray<int>* _func_indexes;

  public:
    JBoltCluster();
    JBoltCluster(const JBoltFunc& func);
    JBoltCluster(const JBoltCluster& cluster);

    bool operator==(const JBoltCluster& cluster) const {
        if (_id != cluster.id())    return false;

        int count = get_funcs_count();
        if (count != cluster.get_funcs_count())
            return false;

        for (int i = 0; i < count; ++i) {
            if (_func_indexes->at(i) != cluster._func_indexes->at(i)) {
            return false;
            }
        }

        return true;
    }

    JBoltCluster& operator=(const JBoltCluster& cluster) {
        _id = cluster.id();
        _heats = cluster.heats();
        _frozen = cluster.frozen();
        _size = cluster.size();
        _density = cluster.density();
        if (_func_indexes != nullptr) {
            delete _func_indexes;
        }
        _func_indexes = create_growable_array<int>(cluster.get_funcs_count());
        _func_indexes->appendAll(cluster.func_indexes());
        return *this;
    }

    virtual ~JBoltCluster() { delete _func_indexes; }

    int id() const                                   { return _id;                                }
    int64_t heats() const                            { return _heats;                             }
    bool frozen() const                              { return _frozen;                            }
    int size() const                                 { return _size;                              }
    double density() const                           { return _density;                           }
    GrowableArray<int>* func_indexes() const         { return _func_indexes;                      }
    int get_funcs_count() const                      { return _func_indexes->length();            }

    void add_heat(int64_t heat);
    void freeze();
    void add_size(int size);
    void update_density();
    void append_func_index(int index);
    void clear();

    static JBoltCluster* find_cluster_by_id(GrowableArray<JBoltCluster>* clusters, u4 id);

    static ByteSize id_offset()                       { return byte_offset_of(JBoltCluster, _id);            }
    static ByteSize heats_offset()                    { return byte_offset_of(JBoltCluster, _heats);         }
    static ByteSize frozen_offset()                   { return byte_offset_of(JBoltCluster, _frozen);        }
    static ByteSize size_offset()                     { return byte_offset_of(JBoltCluster, _size);          }
    static ByteSize density_offset()                  { return byte_offset_of(JBoltCluster, _density);       }
    static ByteSize func_indexes_offset()             { return byte_offset_of(JBoltCluster, _func_indexes);  }

    static JBoltCluster* constructor(const JBoltFunc* func);
    static JBoltCluster* copy_constructor(const JBoltCluster* cluster);
};

class JBoltCall : public CHeapObj<mtTracing> {
  private:
    int _caller_index;
    int _callee_index;
    u4 _call_count;
    traceid _stacktrace_id;

  public:
    JBoltCall();
    JBoltCall(const JBoltCall& call);
    JBoltCall(const JBoltFunc& caller_func, const JBoltFunc& callee_func, u4 call_count, traceid stacktrace_id);

    bool operator==(const JBoltCall& call) const {
        return _caller_index == call._caller_index && _callee_index == call._callee_index;
    }

    JBoltCall& operator=(const JBoltCall& call) {
        _caller_index = call._caller_index;
        _callee_index = call._callee_index;
        _call_count = call._call_count;
        _stacktrace_id = call._stacktrace_id;
        return *this;
    }

    virtual ~JBoltCall() {}

    int caller_index() const             { return _caller_index;                  }
    int callee_index() const             { return _callee_index;                  }
    u4 call_count() const                { return _call_count;                    }
    traceid stacktrace_id() const        { return _stacktrace_id;                 }

    JBoltFunc& caller() const;
    JBoltFunc& callee() const;
    void set_caller_index(int index);
    void set_callee_index(int index);
    void set_call_count(u4 count);

    static ByteSize caller_offset()                       { return byte_offset_of(JBoltCall, _caller_index);            }
    static ByteSize callee_offset()                       { return byte_offset_of(JBoltCall, _caller_index);            }
    static ByteSize call_count_offset()                   { return byte_offset_of(JBoltCall, _call_count);              }
    static ByteSize stacktrace_id_offset()                { return byte_offset_of(JBoltCall, _stacktrace_id);           }

    static JBoltCall* constructor(const JBoltFunc* caller_func, const JBoltFunc* callee_func, u4 call_count, traceid stacktrace_id);
    static JBoltCall* copy_constructor(const JBoltCall* call);
};

#endif // SHARE_JBOLT_JBOLTCALLGRAPH_HPP
