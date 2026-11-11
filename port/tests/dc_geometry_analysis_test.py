from pathlib import Path
import importlib.util
path=Path(__file__).resolve().parents[1]/'private/flycast-research/analyze-dc-geometry.py'
spec=importlib.util.spec_from_file_location('dc_geometry_analysis',path)
analysis=importlib.util.module_from_spec(spec);spec.loader.exec_module(analysis)
vertices=[(10,10,.01,0,0),(20,10,.02,0,0),(20,20,.03,0,0),(100,100,.01,0,0),(120,100,.02,0,0),(120,120,.03,0,0)]
frame=dict(meta={},verts=vertices,idx=[0,1,2,3,4,5],polys=[(2,3,3,0,0,0,0,0)])
assert not list(analysis.triangles(frame)) # old dump TR has unknown storage
frame['meta']={'render_passes':[[1,0,0,1,1]],'sorted_triangles':[[0,0,3]]}
assert list(analysis.triangles(frame))[0][1][0][0]==10 # actual sorted draw batch, not pp.first index
frame['meta']={'render_passes':[[0,0,0,1,0]],'sorted_triangles':[]}
assert list(analysis.triangles(frame))[0][1][0][0]==100 # native unsorted index range
frame['polys']=[(0,0,7,0,0,0,0,0)];frame['idx']=[0,1,2,0xffffffff,3,4,5]
assert len(list(analysis.triangles(frame)))==2
assert analysis.intersects_raster([(10,10),(100,10),(100,100)])
assert analysis.intersects_raster([(-30,10),(20,10),(20,100)])
assert not analysis.intersects_raster([(-30,10),(-20,10),(-20,100)])
assert not analysis.intersects_raster([(-10,0),(0,-10),(-20,-20)])
print('Sorted/unsorted TR draw semantics, unknown-storage exclusion, strip restart and exact raster intersection passed')
